#include "utils/GltfOptimizer.h"
#include <osgUtil/TransformAttributeFunctor>
#include <osgUtil/Tessellator>
#include <osgUtil/Statistics>
#include <osgUtil/MeshOptimizers>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <unordered_set>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgdb_gltf/material/GltfPbrSGMaterial.h>
#include <numeric>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#ifndef OSG_GIS_PLUGINS_PATH_SPLIT_STRING
#ifdef _WIN32
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "\\"
#else
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "/"
#endif
#endif // !OSG_GIS_PLUGINS_PATH_SPLIT_STRING

using namespace osgGISPlugins;

namespace {
// 纹理落盘仪表（AUDIT A-47 判据①：CPU 秒要有归属）。
// 为什么要单独量：exportImage 里那次 writeImageFile 对 .ktx2 会走进
// ktxTexture2_CompressBasisEx，而它自己起 threadCount =
// hardware_concurrency 个线程（src/osgdb_ktx/LoadTextureKTX.cpp:630）。
// 生产侧所有 micro_* 仪表都是**调用线程**的 CPU 秒，这批第三方线程一秒都不在里面。
// 所以这里同时记调用线程秒与**进程**秒：两者之差才是这些线程吃掉的量。
// 进程口径在写出期间会把并行瓦片的 worker 秒也算进来，因此它是上界；
// 用作归因时看的是"差值是否与未归属量同数量级"。
std::atomic<unsigned long long> g_texFiles{ 0 };
std::atomic<unsigned long long> g_texWallNs{ 0 };
std::atomic<unsigned long long> g_texThreadNs{ 0 };
std::atomic<unsigned long long> g_texProcessNs{ 0 };

double fileTimeSec(const FILETIME& ft)
{
	ULARGE_INTEGER v;
	v.LowPart = ft.dwLowDateTime;
	v.HighPart = ft.dwHighDateTime;
	return static_cast<double>(v.QuadPart) / 1.0e7;
}

double currentProcessCpuSec()
{
#ifdef _WIN32
	FILETIME creation, exitT, kernel, user;
	if (GetProcessTimes(GetCurrentProcess(), &creation, &exitT, &kernel, &user))
	{
		return fileTimeSec(user) + fileTimeSec(kernel);
	}
#endif
	return 0.0;
}

double currentThreadCpuSec()
{
#ifdef _WIN32
	FILETIME creation, exitT, kernel, user;
	if (GetThreadTimes(GetCurrentThread(), &creation, &exitT, &kernel, &user))
	{
		return fileTimeSec(user) + fileTimeSec(kernel);
	}
#endif
	return 0.0;
}

class TextureExportProbe {
public:
	TextureExportProbe()
		: _t0(std::chrono::steady_clock::now())
		, _process0(currentProcessCpuSec())
		, _thread0(currentThreadCpuSec())
	{
	}
	void finish()
	{
		using namespace std::chrono;
		g_texWallNs += static_cast<unsigned long long>(
			duration_cast<nanoseconds>(steady_clock::now() - _t0).count());
		g_texThreadNs += static_cast<unsigned long long>((currentThreadCpuSec() - _thread0) * 1.0e9);
		g_texProcessNs += static_cast<unsigned long long>((currentProcessCpuSec() - _process0) * 1.0e9);
		g_texFiles += 1;
	}
private:
	std::chrono::steady_clock::time_point _t0;
	double _process0 = 0.0;
	double _thread0 = 0.0;
};
}

GltfOptimizer::TextureExportStats GltfOptimizer::textureExportStats()
{
	TextureExportStats s;
	s.files = g_texFiles.load();
	s.wallSeconds = static_cast<double>(g_texWallNs.load()) / 1.0e9;
	s.threadCpuSeconds = static_cast<double>(g_texThreadNs.load()) / 1.0e9;
	s.processCpuSeconds = static_cast<double>(g_texProcessNs.load()) / 1.0e9;
	return s;
}

void GltfOptimizer::resetTextureExportStats()
{
	g_texFiles = 0;
	g_texWallNs = 0;
	g_texThreadNs = 0;
	g_texProcessNs = 0;
}

namespace {

// A-51 方案 A 的数据结构：贴图变换在 OSG 侧有两个可能的宿主 ——
// 普通 osg::Material + unit0 贴图路径把图集子矩形写在 osg::Texture 上
// （Osg2Gltf::getOsgTexture2Material），pbr 路径写在 GltfMaterial 上
// （Osg2Gltf::getOsgMaterial2Material）。两边都只认 enable 键为 true 的槽。
// 键值类型必须与图集打包时 setUserValue 用的字面类型逐一对上
// （bool / double / int）：osg::Object::getUserValue 类型不匹配时静默返回 false，
// 读侧就会拿到默认值。
struct TextureTransform
{
	double offsetX = 0.0;
	double offsetY = 0.0;
	double scaleX = 1.0;
	double scaleY = 1.0;
	int texCoord = 0;

	bool isIdentity() const
	{
		return offsetX == 0.0 && offsetY == 0.0 && scaleX == 1.0 && scaleY == 1.0;
	}
	bool sameAs(const TextureTransform& other) const
	{
		return offsetX == other.offsetX && offsetY == other.offsetY &&
			scaleX == other.scaleX && scaleY == other.scaleY &&
			texCoord == other.texCoord;
	}
};

struct TransformSlot
{
	osg::ref_ptr<osg::Object> holder;
	std::string enableKey;
	std::string offsetXKey;
	std::string offsetYKey;
	std::string scaleXKey;
	std::string scaleYKey;
	std::string texCoordKey;
};

static TransformSlot makeTransformSlot(osg::Object* holder,
	const char* enableKey, const char* offsetXKey, const char* offsetYKey,
	const char* scaleXKey, const char* scaleYKey, const char* texCoordKey)
{
	TransformSlot slot;
	slot.holder = holder;
	slot.enableKey = enableKey;
	slot.offsetXKey = offsetXKey;
	slot.offsetYKey = offsetYKey;
	slot.scaleXKey = scaleXKey;
	slot.scaleYKey = scaleYKey;
	slot.texCoordKey = texCoordKey;
	return slot;
}

// 与 include/osgdb_gltf/Extensions.h 的键名成对：tag 取 BASECOLOR / NORMAL /
// OCCLUSION / EMISSIVE / MR / DIFFUSE / SG。
#define OSGGIS_TRANSFORM_SLOT(holderExpr, tag)                                       \
	makeTransformSlot(holderExpr, TEX_TRANSFORM_##tag##_TEXTURE_NAME,                 \
		TEX_TRANSFORM_##tag##_OFFSET_X, TEX_TRANSFORM_##tag##_OFFSET_Y,               \
		TEX_TRANSFORM_##tag##_SCALE_X, TEX_TRANSFORM_##tag##_SCALE_Y,                 \
		TEX_TRANSFORM_##tag##_TEXCOORD)

static bool transformSlotEnabled(const TransformSlot& slot)
{
	bool enabled = false;
	if (!slot.holder.valid()) { return false; }
	slot.holder->getUserValue(slot.enableKey, enabled);
	return enabled;
}

static void readTransformSlot(const TransformSlot& slot, double& offsetX, double& offsetY,
                              double& scaleX, double& scaleY, int& texCoord)
{
	slot.holder->getUserValue(slot.offsetXKey, offsetX);
	slot.holder->getUserValue(slot.offsetYKey, offsetY);
	slot.holder->getUserValue(slot.scaleXKey, scaleX);
	slot.holder->getUserValue(slot.scaleYKey, scaleY);
	slot.holder->getUserValue(slot.texCoordKey, texCoord);
}

static void clearTransformSlot(const TransformSlot& slot)
{
	// 只清 enable 键：写手（getOsgTexture2Material / getOsgMaterial2Material）
	// 见 enableExtension=false 就沿用 offsetX=0,offsetY=0,scaleX=1,scaleY=1 的
	// 局部默认值，再统一乘一次 flipGltfTextureYAxis。因此"清键"与"折进 UV"
	// 必须同时发生，单独清键会让这个 drawable 采到图集的错误位置。
	slot.holder->setUserValue(slot.enableKey, false);
}

class TextureTransformFolder : public osg::NodeVisitor
{
public:
	explicit TextureTransformFolder(GltfOptimizer::TextureUvBackup& backup)
		: osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
		, _backup(backup)
	{
	}

	// 把 uvBackup 里的原始 TEXCOORD_0 装回各自的几何。
	// 条目持几何引用，所以裸 key 与 entry.geometry 必然同地址，直接按条目走即可。
	unsigned restore()
	{
		unsigned restored = 0;
		for (auto& entry : _backup)
		{
			if (entry.second.geometry.valid() && entry.second.originalTexCoords.valid())
			{
				entry.second.geometry->setTexCoordArray(0, entry.second.originalTexCoords.get());
			}
			// 键值也要装回：折叠对恒等变换唯一的改动就是清键，
			// 还原之后节点必须和折叠前逐值相同。
			for (const GltfOptimizer::FoldedSlotState& slot : entry.second.slots)
			{
				if (!slot.holder.valid()) { continue; }
				slot.holder->setUserValue(slot.enableKey, slot.enabled);
				slot.holder->setUserValue(slot.offsetXKey, slot.offsetX);
				slot.holder->setUserValue(slot.offsetYKey, slot.offsetY);
				slot.holder->setUserValue(slot.scaleXKey, slot.scaleX);
				slot.holder->setUserValue(slot.scaleYKey, slot.scaleY);
				slot.holder->setUserValue(slot.texCoordKey, slot.texCoord);
			}
			++restored;
		}
		_backup.clear();
		return restored;
	}

	struct Candidate
	{
		osg::ref_ptr<osg::Geometry> geom;
		std::vector<TransformSlot> slots;
		TextureTransform transform;
		bool foldable = false;
		bool needsBake = false;

		std::vector<osg::Object*> holderKeys() const
		{
			std::vector<osg::Object*> keys;
			keys.reserve(slots.size());
			for (const TransformSlot& s : slots) { keys.push_back(s.holder.get()); }
			return keys;
		}
	};

	unsigned fold(osg::Node* node)
	{
		if (!node) { return 0; }
		_candidates.clear();
		// 契约：一次 (optimize → write) 只折一遍，且折叠前 UV 必须是原始值。
		// 先整体还原上一轮的折叠结果，本轮的烘焙永远建立在原始 UV 上，
		// 变换不可能叠两次。
		restore();
		node->accept(*this);

		// 一个宿主对象（贴图或材质）可能被多个 drawable 引用，变换值就存在宿主上，
		// 所以引用它的 drawable 天然读到同一个变换。清 enable 键会影响该宿主的全部
		// 引用者 —— 只有当每个引用者都能折时才允许清。
		// 反例：贴图被两个 drawable 共用，其中一个 TEXCOORD_0 是 Vec3Array，
		// 单独清共享宿主的键会让它按原始 UV 采到图集里的错误子矩形。
		std::unordered_map<osg::Object*, bool> holderAllFoldable;
		for (const Candidate& c : _candidates)
		{
			for (osg::Object* key : c.holderKeys())
			{
				auto it = holderAllFoldable.find(key);
				if (it == holderAllFoldable.end()) { holderAllFoldable[key] = c.foldable; }
				else { it->second = it->second && c.foldable; }
			}
		}

		unsigned folded = 0;
		for (Candidate& c : _candidates)
		{
			bool apply = c.foldable;
			if (apply)
			{
				for (osg::Object* key : c.holderKeys())
				{
					if (!holderAllFoldable[key]) { apply = false; break; }
				}
			}
			if (applyOne(c, apply)) { ++folded; }
		}
		return folded;
	}

	void apply(osg::Drawable& drawable) override
	{
		osg::Geometry* geom = drawable.asGeometry();
		if (!geom) { return; }

		Candidate c;
		c.geom = geom;
		osg::StateSet* ss = effectiveStateSet(drawable);
		if (ss)
		{
			collectEnabledSlots(*ss, c.slots);
		}
		if (!c.slots.empty())
		{
			bool uniform = true;
			bool scalesValid = true;
			bool texCoordIsZero = true;
			bool firstSlot = true;
			for (const TransformSlot& slot : c.slots)
			{
				TextureTransform t;
				readTransformSlot(slot, t.offsetX, t.offsetY, t.scaleX, t.scaleY, t.texCoord);
				if (firstSlot) { c.transform = t; firstSlot = false; }
				else if (!c.transform.sameAs(t)) { uniform = false; }
				if (t.scaleX <= 0.0 || t.scaleY <= 0.0) { scalesValid = false; }
				if (t.texCoord != 0) { texCoordIsZero = false; }
			}
			// 恒等变换不需要改写 UV：清掉 enable 键后写手仍会输出同一个（翻转后的）
			// 恒等 extension，材质值不变，但省掉"这个槽还在生效"造成的材质分裂。
			c.needsBake = !(uniform && c.transform.isIdentity());
			c.foldable = uniform && scalesValid && texCoordIsZero &&
				(!c.needsBake || hasVec2TexCoords(*geom));
		}
		_candidates.push_back(c);
	}

private:
	// 写手（Osg2Gltf::pushStateSet/_ssStack）取的是"drawable 自己的 StateSet，
	// 没有则继承最近一个非空父节点 StateSet"，这里按同一条规则解析。
	// 父链用 getParent(0) 并有深度上限：多父节点（共享子图）时无法唯一确定继承链，
	// 与写手存在偏差，此时宁可因为读不到键而不折叠。
	static osg::StateSet* effectiveStateSet(osg::Node& node)
	{
		if (node.getStateSet()) { return node.getStateSet(); }
		for (unsigned i = 0; i < node.getNumParents(); ++i)
		{
			osg::Node* parent = node.getParent(i);
			for (int depth = 0; parent && depth < 256; ++depth)
			{
				if (parent->getStateSet()) { return parent->getStateSet(); }
				parent = parent->getNumParents() ? parent->getParent(0) : nullptr;
			}
		}
		return nullptr;
	}

	static void collectEnabledSlots(osg::StateSet& ss, std::vector<TransformSlot>& out)
	{
		const osg::ref_ptr<osg::Texture> texture = dynamic_cast<osg::Texture*>(
			ss.getTextureAttribute(0, osg::StateAttribute::TEXTURE));
		if (texture.valid())
		{
			const TransformSlot basecolor =
				OSGGIS_TRANSFORM_SLOT(static_cast<osg::Object*>(texture.get()), BASECOLOR);
			if (transformSlotEnabled(basecolor)) { out.push_back(basecolor); }
		}

		const osg::ref_ptr<osg::Material> material =
			dynamic_cast<osg::Material*>(ss.getAttribute(osg::StateAttribute::MATERIAL));
		const osg::ref_ptr<GltfMaterial> gltfMaterial =
			material.valid() ? dynamic_cast<GltfMaterial*>(material.get()) : nullptr;
		if (gltfMaterial.valid())
		{
			osg::Object* holder = static_cast<osg::Object*>(gltfMaterial.get());
			const TransformSlot slots[] = {
				OSGGIS_TRANSFORM_SLOT(holder, BASECOLOR),
				OSGGIS_TRANSFORM_SLOT(holder, NORMAL),
				OSGGIS_TRANSFORM_SLOT(holder, OCCLUSION),
				OSGGIS_TRANSFORM_SLOT(holder, EMISSIVE),
				OSGGIS_TRANSFORM_SLOT(holder, DIFFUSE),
				OSGGIS_TRANSFORM_SLOT(holder, SG),
			};
			for (const TransformSlot& slot : slots)
			{
				if (transformSlotEnabled(slot)) { out.push_back(slot); }
			}
			if (dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get()))
			{
				const TransformSlot mr = OSGGIS_TRANSFORM_SLOT(holder, MR);
				if (transformSlotEnabled(mr)) { out.push_back(mr); }
			}
		}
	}

	static bool hasVec2TexCoords(const osg::Geometry& geom)
	{
		return dynamic_cast<const osg::Vec2Array*>(geom.getTexCoordArray(0)) != nullptr;
	}

	// 只在"手上这份数组是原始 UV"的前提下烘焙（fold() 已先整体还原）。
	bool applyOne(Candidate& c, bool apply)
	{
		if (!apply) { return false; }

		const osg::ref_ptr<osg::Vec2Array> original =
			dynamic_cast<osg::Vec2Array*>(c.geom->getTexCoordArray(0));
		if (c.needsBake && !original.valid()) { return false; }

		GltfOptimizer::TextureUvBackupEntry entry;
		entry.geometry = c.geom;
		entry.originalTexCoords = original;
		for (const TransformSlot& slot : c.slots)
		{
			GltfOptimizer::FoldedSlotState saved;
			saved.holder = slot.holder;
			saved.enableKey = slot.enableKey;
			saved.offsetXKey = slot.offsetXKey;
			saved.offsetYKey = slot.offsetYKey;
			saved.scaleXKey = slot.scaleXKey;
			saved.scaleYKey = slot.scaleYKey;
			saved.texCoordKey = slot.texCoordKey;
			saved.enabled = true;
			readTransformSlot(slot, saved.offsetX, saved.offsetY, saved.scaleX,
			                  saved.scaleY, saved.texCoord);
			entry.slots.push_back(saved);
			clearTransformSlot(slot);
		}
		_backup[c.geom.get()] = entry;
		if (!c.needsBake) { return true; }

		osg::ref_ptr<osg::Vec2Array> baked = new osg::Vec2Array;
		baked->reserve(original->size());
		for (const osg::Vec2& uv : *original.get())
		{
			// OSG 口径的正变换：u' = sx·u + ox, v' = sy·v + oy。
			// 写手随后无条件乘的那次 Y 翻转（offsetY=1-offsetY, scaleY=-scaleY）
			// 在折叠前后都同样作用，所以采到的是同一个纹素；
			// 差异只有 float 舍入（把乘加折进一次转换），在 ≤512px 的代理贴图上是亚像素量。
			baked->push_back(osg::Vec2(
				static_cast<float>(c.transform.scaleX * uv.x() + c.transform.offsetX),
				static_cast<float>(c.transform.scaleY * uv.y() + c.transform.offsetY)));
		}
		// 新数组，不改原数组：和 osg::Image 一样，TEXCOORD_0 在瓦片克隆时也可能被共用。
		c.geom->setTexCoordArray(0, baked.get());
		return true;
	}

	GltfOptimizer::TextureUvBackup& _backup;
	std::vector<Candidate> _candidates;
};

} // namespace

unsigned GltfOptimizer::foldTextureTransformsIntoUv(osg::Node* node, TextureUvBackup& uvBackup)
{
	TextureTransformFolder folder(uvBackup);
	return folder.fold(node);
}

unsigned GltfOptimizer::restoreFoldedTextureUvs(TextureUvBackup& uvBackup)
{
	TextureTransformFolder folder(uvBackup);
	return folder.restore();
}

void GltfOptimizer::optimize(osg::Node* node, unsigned int options)
{
	osgUtil::StatsVisitor stats;

	if (osg::getNotifyLevel() >= osg::INFO)
	{
		node->accept(stats);
		stats.totalUpStats();
		OSG_NOTICE << std::endl << "Stats before:" << std::endl;
		stats.print(osg::notify(osg::NOTICE));
	}

	if (options & MERGE_TRANSFORMS)
	{
		OSG_INFO << "Optimizer::optimize() doing MERGE_TRANSFORMS" << std::endl;
		// 1. 统一获取一次 group 指针，避免重复定义
		osg::Group* group = node->asGroup();
		if (group)
		{
			MergeTransformVisitor mtv;
			node->accept(mtv);

			// 2. 检查访问器结果是否有效
			osg::Node* visitorNode = mtv.getNode();
			if (visitorNode)
			{
				osg::Group* newGroup = visitorNode->asGroup();
				if (newGroup)
				{
					// 3. 移除旧节点
					group->removeChildren(0, group->getNumChildren());

					// 4. 优化节点转移：
					// 使用 range-based for 遍历所有子节点（需 OSG 支持迭代器，或手动处理）
					// 另一种更稳健的方法是直接将子节点列表移动过来
					unsigned int numChildren = newGroup->getNumChildren();
					for (unsigned int i = 0; i < numChildren; ++i)
					{
						group->addChild(newGroup->getChild(i));
					}
				}
			}
		}
	}

	if ((options & GENERATE_NORMAL_TEXTURE)) {
		OSG_INFO << "Optimizer::optimize() doing GENERATE_NORMAL_TEXTURE" << std::endl;
		GenerateNormalTextureVisitor gntv(this);
		node->accept(gntv);
	}


	if (options & TEXTURE_ATLAS_BUILDER_BY_STB)
	{
		OSG_INFO << "Optimizer::optimize() doing TEXTURE_ATLAS_BUILDER by stb library" << std::endl;
		TextureAtlasBuilderVisitor tabv(_gltfTextureOptions, this);
		node->accept(tabv);
		tabv.packTextures();


		// now merge duplicate state, that may have been introduced by merge textures into texture atlas'
		bool combineDynamicState = false;
		bool combineStaticState = true;
		bool combineUnspecifiedState = true;

		osgUtil::Optimizer::StateVisitor osv(combineDynamicState, combineStaticState, combineUnspecifiedState, this);
		node->accept(osv);
		osv.optimize();
	}

	if (options & STATIC_OBJECT_DETECTION)
	{
		osgUtil::Optimizer::StaticObjectDetectionVisitor sodv;
		node->accept(sodv);
	}

	if (options & TESSELLATE_GEOMETRY)
	{
		OSG_INFO << "Optimizer::optimize() doing TESSELLATE_GEOMETRY" << std::endl;

		osgUtil::Optimizer::TessellateVisitor tsv;
		node->accept(tsv);
	}

	if (options & REMOVE_LOADED_PROXY_NODES)
	{
		OSG_INFO << "Optimizer::optimize() doing REMOVE_LOADED_PROXY_NODES" << std::endl;

		osgUtil::Optimizer::RemoveLoadedProxyNodesVisitor rlpnv(this);
		node->accept(rlpnv);
		rlpnv.removeRedundantNodes();

	}

	if (options & COMBINE_ADJACENT_LODS)
	{
		OSG_INFO << "Optimizer::optimize() doing COMBINE_ADJACENT_LODS" << std::endl;

		osgUtil::Optimizer::CombineLODsVisitor clv(this);
		node->accept(clv);
		clv.combineLODs();
	}

	if (options & OPTIMIZE_TEXTURE_SETTINGS)
	{
		OSG_INFO << "Optimizer::optimize() doing OPTIMIZE_TEXTURE_SETTINGS" << std::endl;

		osgUtil::Optimizer::TextureVisitor tv(true, true, // unref image
			false, false, // client storage
			false, 1.0, // anisotropic filtering
			this);
		node->accept(tv);
	}

	if (options & SHARE_DUPLICATE_STATE)
	{
		OSG_INFO << "Optimizer::optimize() doing SHARE_DUPLICATE_STATE" << std::endl;

		bool combineDynamicState = false;
		bool combineStaticState = true;
		bool combineUnspecifiedState = true;

		osgUtil::Optimizer::StateVisitor osv(combineDynamicState, combineStaticState, combineUnspecifiedState, this);
		node->accept(osv);
		osv.optimize();
	}

	if ((options & TEXTURE_ATLAS_BUILDER) && !(options & TEXTURE_ATLAS_BUILDER_BY_STB))
	{
		OSG_INFO << "Optimizer::optimize() doing TEXTURE_ATLAS_BUILDER" << std::endl;

		// traverse the scene collecting textures into texture atlas.
		osgUtil::Optimizer::TextureAtlasVisitor tav(this);
		node->accept(tav);
		tav.optimize();

		// now merge duplicate state, that may have been introduced by merge textures into texture atlas'
		bool combineDynamicState = false;
		bool combineStaticState = true;
		bool combineUnspecifiedState = true;

		osgUtil::Optimizer::StateVisitor osv(combineDynamicState, combineStaticState, combineUnspecifiedState, this);
		node->accept(osv);
		osv.optimize();
	}


	if (options & COPY_SHARED_NODES)
	{
		OSG_INFO << "Optimizer::optimize() doing COPY_SHARED_NODES" << std::endl;

		osgUtil::Optimizer::CopySharedSubgraphsVisitor cssv(this);
		node->accept(cssv);
		cssv.copySharedNodes();
	}

	if (options & FLATTEN_STATIC_TRANSFORMS)
	{
		OSG_INFO << "Optimizer::optimize() doing FLATTEN_STATIC_TRANSFORMS" << std::endl;

		int i = 0;
		bool result = false;
		do
		{
			OSG_DEBUG << "** RemoveStaticTransformsVisitor *** Pass " << i << std::endl;
			osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv(this);
			node->accept(fstv);
			result = fstv.removeTransforms(node);
			++i;
		} while (result);

		// now combine any adjacent static transforms.
		osgUtil::Optimizer::CombineStaticTransformsVisitor cstv(this);
		node->accept(cstv);
		cstv.removeTransforms(node);
	}
	else if (options & FLATTEN_TRANSFORMS)
	{
		OSG_INFO << "Optimizer::optimize() doing FLATTEN_TRANSFORMS" << std::endl;
		FlattenTransformsVisitor ftv(this);
		node->accept(ftv);
	}

	if (options & FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS)
	{
		OSG_INFO << "Optimizer::optimize() doing FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS" << std::endl;

		// now combine any adjacent static transforms.
		osgUtil::Optimizer::FlattenStaticTransformsDuplicatingSharedSubgraphsVisitor fstdssv(this);
		node->accept(fstdssv);

	}

	if (options & REMOVE_REDUNDANT_NODES)
	{
		OSG_INFO << "Optimizer::optimize() doing REMOVE_REDUNDANT_NODES" << std::endl;

		osgUtil::Optimizer::RemoveEmptyNodesVisitor renv(this);
		node->accept(renv);
		renv.removeEmptyNodes();

		osgUtil::Optimizer::RemoveRedundantNodesVisitor rrnv(this);
		node->accept(rrnv);
		rrnv.removeRedundantNodes();

	}

	if (options & MERGE_GEODES)
	{
		OSG_INFO << "Optimizer::optimize() doing MERGE_GEODES" << std::endl;

		osg::Timer_t startTick = osg::Timer::instance()->tick();

		osgUtil::Optimizer::MergeGeodesVisitor visitor;
		node->accept(visitor);

		osg::Timer_t endTick = osg::Timer::instance()->tick();

		OSG_INFO << "MERGE_GEODES took " << osg::Timer::instance()->delta_s(startTick, endTick) << std::endl;
	}

	if (options & MAKE_FAST_GEOMETRY)
	{
		OSG_INFO << "Optimizer::optimize() doing MAKE_FAST_GEOMETRY" << std::endl;

		osgUtil::Optimizer::MakeFastGeometryVisitor mgv(this);
		node->accept(mgv);
	}

	if (options & MERGE_GEOMETRY)
	{
		OSG_INFO << "Optimizer::optimize() doing MERGE_GEOMETRY" << std::endl;

		osg::Timer_t startTick = osg::Timer::instance()->tick();

		osgUtil::Optimizer::MergeGeometryVisitor mgv(this);
		mgv.setTargetMaximumNumberOfVertices(10000);
		node->accept(mgv);

		osg::Timer_t endTick = osg::Timer::instance()->tick();

		OSG_INFO << "MERGE_GEOMETRY took " << osg::Timer::instance()->delta_s(startTick, endTick) << std::endl;
	}


	if (options & FLATTEN_BILLBOARDS)
	{
		osgUtil::Optimizer::FlattenBillboardVisitor fbv(this);
		node->accept(fbv);
		fbv.process();
	}

	if (options & SPATIALIZE_GROUPS)
	{
		OSG_INFO << "Optimizer::optimize() doing SPATIALIZE_GROUPS" << std::endl;

		osgUtil::Optimizer::SpatializeGroupsVisitor sv(this);
		node->accept(sv);
		sv.divide();
	}

	if (options & INDEX_MESH)
	{
		OSG_INFO << "Optimizer::optimize() doing INDEX_MESH" << std::endl;
		osgUtil::IndexMeshVisitor imv(this);
		node->accept(imv);
		imv.makeMesh();
	}
	else if (options & INDEX_MESH_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing INDEX_MESH by meshoptimizer library" << std::endl;
		osgUtil::IndexMeshVisitor imv(this);
		node->accept(imv);
		imv.makeMesh();
		ReindexMeshVisitor rmv(this);
		node->accept(rmv);
	}

	if (options & VERTEX_POSTTRANSFORM)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_POSTTRANSFORM" << std::endl;
		osgUtil::VertexCacheVisitor vcv;
		node->accept(vcv);
		vcv.optimizeVertices();
	}

	if (options & VERTEX_PRETRANSFORM)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_PRETRANSFORM" << std::endl;
		osgUtil::VertexAccessOrderVisitor vaov;
		node->accept(vaov);
		vaov.optimizeOrder();
	}

	if (options & BUFFER_OBJECT_SETTINGS)
	{
		OSG_INFO << "Optimizer::optimize() doing BUFFER_OBJECT_SETTINGS" << std::endl;
		osgUtil::Optimizer::BufferObjectVisitor bov(true, true, true, true, true, false);
		node->accept(bov);
	}

	if (options & VERTEX_CACHE_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_CACHE_BY_MESHOPTIMIZER by meshoptimizer library" << std::endl;
		VertexCacheVisitor vcv(this);
		node->accept(vcv);
	}

	if (options & OVER_DRAW_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing OVER_DRAW_BY_MESHOPTIMIZER by meshoptimizer library" << std::endl;
		OverDrawVisitor odv(this);
		node->accept(odv);
	}

	if (options & VERTEX_FETCH_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_FETCH_BY_MESHOPTIMIZER by meshoptimizer library" << std::endl;
		VertexFetchVisitor vfv(this);
		node->accept(vfv);
	}

	if ((options & GENERATE_NORMAL_TEXTURE)) {
		OSG_INFO << "Optimizer::optimize() doing GENERATE_NORMAL_TEXTURE" << std::endl;
		GenerateTangentVisitor gtv(this);
		node->accept(gtv);
	}

	if (osg::getNotifyLevel() >= osg::INFO)
	{
		stats.reset();
		node->accept(stats);
		stats.totalUpStats();
		OSG_NOTICE << std::endl << "Stats after:" << std::endl;
		stats.print(osg::notify(osg::NOTICE));
	}
}
/** GenerateNormalTextureVisitor */
osg::ref_ptr<osg::Image> GltfOptimizer::GenerateNormalTextureVisitor::getOrGenerateNormalMap(osg::Image* image) {
	if (!image) { return nullptr; }
	std::string filename = image->getFileName();

	auto found = _imageCache.find(filename);
	if (found != _imageCache.end()) { return found->second; }

	osg::ref_ptr<osg::Image> gray = GltfMaterial::convertToGrayscale(image);
	osg::ref_ptr<osg::Image> normalMap = GltfMaterial::generateNormalMap(gray);
	if (normalMap.valid()) {
		_imageCache.insert({ filename, normalMap });
	}
	return normalMap;
}

osg::ref_ptr<osg::Image> GltfOptimizer::GenerateNormalTextureVisitor::getOrGenerateMetallicRoughnessMap(osg::Image* image)
{
	if (!image) { return nullptr; }
	std::string filename = image->getFileName();

	auto found = _imageCache2.find(filename);
	if (found != _imageCache2.end()) { return found->second; }

	osg::ref_ptr<osg::Image> metallicRoughnessMap = GltfPbrMRMaterial::generateMetallicRoughnessMap(image);
	if (metallicRoughnessMap.valid()) {
		_imageCache2.insert({ filename, metallicRoughnessMap });
	}
	return metallicRoughnessMap;
}

void GltfOptimizer::GenerateNormalTextureVisitor::processGeometry(osg::Geometry* geometry) {
	osg::StateSet* stateSet = geometry->getStateSet();
	if (!stateSet) { return; }

	osg::Material* material = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
	if (!material) { return; }

	GltfMaterial* gltfMaterial = dynamic_cast<GltfMaterial*>(material);
	if (!gltfMaterial) { return; }

	osg::Image* baseImage = nullptr;

	if (auto* gltfMR = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial)) {
		if (gltfMR->baseColorTexture.valid()) {
			baseImage = gltfMR->baseColorTexture->getImage();
			if (auto normalMap = getOrGenerateNormalMap(baseImage)) {
				auto tex = new osg::Texture2D(normalMap);
				tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
				tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
				tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
				tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
				gltfMR->normalTexture = tex;
			}
		}
	}
	else if (auto* gltfSG = dynamic_cast<GltfPbrSGMaterial*>(gltfMaterial)) {
		for (auto* ext : gltfSG->materialExtensionsByCesiumSupport) {
			if (auto* specGloss = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(ext)) {
				if (specGloss->osgDiffuseTexture.valid()) {
					baseImage = specGloss->osgDiffuseTexture->getImage();
					if (auto normalMap = getOrGenerateNormalMap(baseImage)) {
						auto tex = new osg::Texture2D(normalMap);
						tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
						tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
						tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
						tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
						gltfSG->normalTexture = tex;
					}
				}
			}
		}
	}
}

void GltfOptimizer::GenerateNormalTextureVisitor::apply(osg::Geode& geode) {
	std::vector<osg::ref_ptr<osg::Geometry>> geometries;
	for (unsigned int i = 0; i < geode.getNumDrawables(); ++i) {
		osg::Geometry* geom = dynamic_cast<osg::Geometry*>(geode.getDrawable(i));
		if (geom) { geometries.emplace_back(geom); }
	}

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, geometries.size()),
		[this, &geometries](const tbb::blocked_range<size_t>& range) {
			for (size_t i = range.begin(); i < range.end(); ++i) {
				processGeometry(geometries[i].get());
			}
		}
	);

	traverse(geode);
}
/** GenerateTangentVisitor */
void GltfOptimizer::GenerateTangentVisitor::generateTangent(osg::Geometry& geometry)
{
	auto* vertices = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	auto* texcoords = dynamic_cast<osg::Vec2Array*>(geometry.getTexCoordArray(0));

	if (!vertices || !texcoords || texcoords->size() != vertices->size()) { return; }
	if (geometry.getNumPrimitiveSets() == 0) { return; }

	osg::DrawElements* indices = nullptr;
	for (unsigned int i = 0; i < geometry.getNumPrimitiveSets(); ++i) {
		indices = dynamic_cast<osg::DrawElements*>(geometry.getPrimitiveSet(i));
		if (indices) { break; }
	}
	if (!indices) { return; }

	osg::ref_ptr<osg::Vec4Array> tangents = new osg::Vec4Array(vertices->size());
	std::vector<osg::Vec3> tan1(vertices->size(), osg::Vec3(0.0f, 0.0f, 0.0f));
	std::vector<osg::Vec3> tan2(vertices->size(), osg::Vec3(0.0f, 0.0f, 0.0f));

	for (unsigned int i = 0; i < indices->getNumIndices(); i += 3) {
		unsigned int i1 = indices->getElement(i);
		unsigned int i2 = indices->getElement(i + 1);
		unsigned int i3 = indices->getElement(i + 2);

		const osg::Vec3& v1 = (*vertices)[i1];
		const osg::Vec3& v2 = (*vertices)[i2];
		const osg::Vec3& v3 = (*vertices)[i3];

		const osg::Vec2& w1 = (*texcoords)[i1];
		const osg::Vec2& w2 = (*texcoords)[i2];
		const osg::Vec2& w3 = (*texcoords)[i3];

		osg::Vec3 edge1 = v2 - v1;
		osg::Vec3 edge2 = v3 - v1;

		float x1 = w2.x() - w1.x();
		float x2 = w3.x() - w1.x();
		float y1 = w2.y() - w1.y();
		float y2 = w3.y() - w1.y();

		float denom = x1 * y2 - x2 * y1;
		if (osg::absolute(denom) < 1e-6f) { // 阈值避免浮点误差
			continue; // 跳过退化三角形
		}

		float r = 1.0f / denom;
		osg::Vec3 sdir((y2 * edge1.x() - y1 * edge2.x()) * r,
			(y2 * edge1.y() - y1 * edge2.y()) * r,
			(y2 * edge1.z() - y1 * edge2.z()) * r);
		osg::Vec3 tdir((x1 * edge2.x() - x2 * edge1.x()) * r,
			(x1 * edge2.y() - x2 * edge1.y()) * r,
			(x1 * edge2.z() - x2 * edge1.z()) * r);

		tan1[i1] += sdir; tan1[i2] += sdir; tan1[i3] += sdir;
		tan2[i1] += tdir; tan2[i2] += tdir; tan2[i3] += tdir;
	}

	osg::Vec3Array* normals = dynamic_cast<osg::Vec3Array*>(geometry.getNormalArray());

	for (unsigned int i = 0; i < vertices->size(); ++i) {
		const osg::Vec3& n = normals ? (*normals)[i] : osg::Vec3(0, 0, 1);
		const osg::Vec3& t = tan1[i];

		osg::Vec3 tangent = (t - n * (n * t));
		tangent.normalize();

		float handedness = ((n ^ t) * tan2[i]) < 0.0f ? -1.0f : 1.0f;
		(*tangents)[i] = osg::Vec4(tangent, handedness);
	}

	tangents->setName("TANGENT");
	geometry.setVertexAttribArray(1, tangents, osg::Array::BIND_PER_VERTEX);
}

void GltfOptimizer::GenerateTangentVisitor::apply(osg::Geometry& geometry) {

	osg::ref_ptr<osg::StateSet> stateSet = geometry.getStateSet();
	if (stateSet.valid())
	{
		const osg::ref_ptr<osg::Material> osgMaterial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		if (osgMaterial.valid())
		{
			osg::ref_ptr<GltfMaterial> gltfMaterial = dynamic_cast<GltfMaterial*>(osgMaterial.get());
			if (gltfMaterial.valid())
			{
				if (gltfMaterial->normalTexture.valid()) {
					generateTangent(geometry);
				}
			}
		}
	}
}
/** FlattenTransformsVisitor */
void GltfOptimizer::FlattenTransformsVisitor::apply(osg::Drawable& drawable)
{
	osgUtil::TransformAttributeFunctor tf(_currentMatrix);
	drawable.accept(tf);
	drawable.dirtyBound();
	drawable.dirtyGLObjects();
}

void GltfOptimizer::FlattenTransformsVisitor::apply(osg::Transform& transform)
{
	osg::Matrixd previousMatrix = _currentMatrix;
	osg::Matrixd localMatrix;
	transform.computeLocalToWorldMatrix(localMatrix, this);
	_currentMatrix.preMult(localMatrix);
	traverse(transform);
	if (osg::MatrixTransform* matrixTransform = transform.asMatrixTransform())
	{
		matrixTransform->setMatrix(osg::Matrixd::identity());
	}
	else if (osg::PositionAttitudeTransform* positionAttitudeTransform = transform.asPositionAttitudeTransform())
	{
		positionAttitudeTransform->setPosition(osg::Vec3());
		positionAttitudeTransform->setAttitude(osg::Quat());
	}

	if (transform.getNumChildren() > 0)
	{
		transform.dirtyBound();
		transform.computeBound();
	}
	_currentMatrix = previousMatrix;
}

/** ReindexMeshVisitor */
void GltfOptimizer::ReindexMeshVisitor::apply(osg::Geometry& geometry)
{
	const unsigned int psetCount = geometry.getNumPrimitiveSets();

	for (unsigned int primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::PrimitiveSet* pset = geometry.getPrimitiveSet(primIndex);
		if (!pset) { continue; }

		// 尝试转换为 UShort 类型
		if (auto* deUShort = dynamic_cast<osg::DrawElementsUShort*>(pset))
		{
			reindexMesh<osg::DrawElementsUShort, osg::UShortArray>(geometry, deUShort, primIndex);
		}
		// 尝试转换为 UInt 类型
		else if (auto* deUInt = dynamic_cast<osg::DrawElementsUInt*>(pset))
		{
			reindexMesh<osg::DrawElementsUInt, osg::UIntArray>(geometry, deUInt, primIndex);
		}
	}
}

/** VertexCacheVisitor */
void GltfOptimizer::VertexCacheVisitor::apply(osg::Geometry& geometry)
{
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) { return; }
	const size_t vertexCount = positions->size();
	if (vertexCount == 0) { return; }
	const unsigned int psetCount = geometry.getNumPrimitiveSets();

	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES)
		{
			const osg::PrimitiveSet::Type type = pset->getType();
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }
				if (_compressmore)
				{//能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
					meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				}
				else
				{
					meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				}

				osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }
				if (_compressmore)
				{//能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
					meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				}
				else
				{
					meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				}

				osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }
				if (_compressmore)
				{//能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
					meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				}
				else
				{
					meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				}

				osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
		}
	}
}

/** OverDrawVisitor */
void GltfOptimizer::OverDrawVisitor::apply(osg::Geometry& geometry)
{
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) { return; }
	const size_t vertexCount = positions->size();
	if (vertexCount == 0) { return; }
	const unsigned int psetCount = geometry.getNumPrimitiveSets();

	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES)
		{
			const osg::PrimitiveSet::Type type = pset->getType();
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }
				meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

				osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
			{

				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }
				osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());

			}
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }
				meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

				osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());

			}
		}
	}
}

/** VertexFetchVisitor */
void GltfOptimizer::VertexFetchVisitor::apply(osg::Geometry& geometry)
{
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) { return; }
	const size_t vertexCount = positions->size();
	if (vertexCount == 0) { return; }
	const unsigned int psetCount = geometry.getNumPrimitiveSets();

	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES)
		{
			const osg::PrimitiveSet::Type type = pset->getType();
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }

				osg::MixinVector<unsigned int> remap(vertexCount);
				size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap.asVector()[0], &indices->asVector()[0], indices->asVector().size(), vertexCount);
				assert(uniqueVertices <= vertexCount);
				osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }

				osg::MixinVector<unsigned int> remap(vertexCount);
				size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap.asVector()[0], &indices->asVector()[0], indices->asVector().size(), vertexCount);

				assert(uniqueVertices <= vertexCount);
				osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
				if (indices->empty()) { continue; }

				osg::MixinVector<unsigned int> remap(vertexCount);
				size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap.asVector()[0], &indices->asVector()[0], indices->asVector().size(), vertexCount);
				assert(uniqueVertices <= vertexCount);
				osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
		}
	}
}

/** TextureAtlasBuilderVisitor */
std::unordered_map<std::string, std::shared_ptr<std::mutex>> GltfOptimizer::TextureAtlasBuilderVisitor::imageFileLocks;  // 定义静态成员
std::shared_mutex GltfOptimizer::TextureAtlasBuilderVisitor::imageFileLocksMapMutex;

std::shared_ptr<std::mutex> osgGISPlugins::GltfOptimizer::TextureAtlasBuilderVisitor::getImageLock(const std::string& filename)
{
	// 尝试使用共享锁查找
	{
		std::shared_lock<std::shared_mutex> readLock(imageFileLocksMapMutex);
		auto it = imageFileLocks.find(filename);
		if (it != imageFileLocks.end())
		{
			return it->second;
		}
	}

	// 写锁：若不存在则创建
	std::unique_lock<std::shared_mutex> writeLock(imageFileLocksMapMutex);
	auto& ptr = imageFileLocks[filename];
	if (!ptr)
	{
		ptr = std::make_shared<std::mutex>();
	}
	return ptr;
}

std::string GltfOptimizer::TextureAtlasBuilderVisitor::exportImage(const osg::ref_ptr<osg::Image>& img, const GltfTextureType type)
{
	std::string ext = _options.ext;
	std::string filename = TexturePacker::computeImageHash(img);
	switch (type)
	{
	case GltfTextureType::NORMAL:
		filename += "-normal";
		break;
	case GltfTextureType::OCCLUSION:
		filename += "-occlusion";
		break;
	case GltfTextureType::EMISSIVE:
		filename += "-emissive";
		break;
	case GltfTextureType::METALLICROUGHNESS:
		filename += "-metallicroughness";
		break;
	case GltfTextureType::BASECOLOR:
		filename += "-basecolor";
		break;
	case GltfTextureType::DIFFUSE:
		filename += "-diffuse";
		break;
	case GltfTextureType::SPECULARGLOSSINESS:
		filename += "-specularglossiness";
		break;
	default:
		break;
	}

	const GLenum pixelFormat = img->getPixelFormat();
	if (_options.ext == ".jpg" && pixelFormat != GL_ALPHA && pixelFormat != GL_RGB)
	{
		ext = ".png";
	}

	std::string basePath = _options.cachePath + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + filename;
	std::string primaryPath = basePath + ext;
	std::string fallbackPath = basePath + ".png";
	std::string fullPath = primaryPath;

	std::shared_ptr<std::mutex> fileLock = getImageLock(basePath);
	std::unique_lock<std::mutex> fileWriteLock(*fileLock);

	std::ifstream fileCheck(fullPath);
	bool fileExists = fileCheck.good() && fileCheck.peek() != std::ifstream::traits_type::eof();
	fileCheck.close();

	if (!fileExists)
	{
		osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
		if (ext == ".jpg" || ext == ".jpeg")
		{
			options->setOptionString("JPEG_QUALITY 75");
		}
		else if (ext == ".png")
		{
			options->setOptionString("PNG_COMPRESSION 5");
		}

		TextureExportProbe probe;
		if (!osgDB::writeImageFile(*img.get(), fullPath, options))
		{
			fullPath = fallbackPath;
			std::ifstream altCheck(fullPath);
			bool altExists = altCheck.good() && altCheck.peek() != std::ifstream::traits_type::eof();
			altCheck.close();
			if (!altExists) {
				options->setOptionString("PNG_COMPRESSION 5");
				if (!osgDB::writeImageFile(*img.get(), fullPath, options))
				{
					osg::notify(osg::FATAL) << "\nFailed to write image: " << fullPath << std::endl;
					fullPath.clear();
				}
			}
		}
		probe.finish();
	}

	if (img->getFileName().empty())
	{
		img->setFileName(fullPath);
	}

	return fullPath;
}

void GltfOptimizer::TextureAtlasBuilderVisitor::apply(osg::Drawable& drawable)
{
	try {
		const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
		osg::ref_ptr<osg::StateSet> oldStateSet = drawable.getStateSet();
		if (oldStateSet.valid())
		{
			// 远景 LOD 代理的 StateSet 已在构建时去纹理（FarLodTextureStripper）。
			// 这类无纹理 StateSet 没有任何可图集化的纹理，克隆+替换毫无意义，
			// 反而会触碰可能与共享对象纠缠的 Uniform 生命周期（历史段错误根因）。
			// 所有纹理单元均无纹理属性时直接跳过。
			bool hasTexture = false;
			const osg::StateSet::TextureAttributeList& tal = oldStateSet->getTextureAttributeList();
			for (const auto& unit : tal)
			{
				for (const auto& kv : unit)
				{
					// kv.second 是 RefAttributePair（kv.first 是 Type key）
					if (kv.second.first.valid() && kv.second.first->asTexture())
					{
						hasTexture = true;
						break;
					}
				}
				if (hasTexture) break;
			}
			// GltfMaterial 自带的非 StateSet 纹理字段（normal/occlusion/emissive 等）
			// 也要查：有它们时仍需走正常图集路径。
			if (!hasTexture)
			{
				const osg::ref_ptr<osg::Material> osgMaterialProbe =
					dynamic_cast<osg::Material*>(oldStateSet->getAttribute(osg::StateAttribute::MATERIAL));
				const osg::ref_ptr<GltfMaterial> gltfMaterialProbe =
					osgMaterialProbe.valid() ? dynamic_cast<GltfMaterial*>(osgMaterialProbe.get()) : nullptr;
				if (gltfMaterialProbe.valid() && gltfMaterialHasTexture(gltfMaterialProbe))
				{
					hasTexture = true;
				}
			}
			if (!hasTexture)
			{
				return;
			}
			osg::ref_ptr<osg::StateSet> stateSet = osg::clone(oldStateSet.get(), osg::CopyOp::DEEP_COPY_STATESETS | osg::CopyOp::DEEP_COPY_STATEATTRIBUTES | osg::CopyOp::DEEP_COPY_TEXTURES);
			drawable.setStateSet(stateSet);
			const osg::ref_ptr<osg::Material> osgMaterial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
			if (osgMaterial.valid())
			{
				osg::ref_ptr<GltfMaterial> gltfMaterial = dynamic_cast<GltfMaterial*>(osgMaterial.get());
				if (gltfMaterial.valid())
				{
					if (gltfMaterialHasTexture(gltfMaterial))
					{
						optimizeOsgMaterial(gltfMaterial, geom);
					}
				}
				else
				{
					optimizeOsgTexture(stateSet, geom);
				}
			}
			else
			{
				optimizeOsgTexture(stateSet, geom);
			}
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packTextures()
{
	packOsgTextures();
	packOsgMaterials();
}

void GltfOptimizer::TextureAtlasBuilderVisitor::optimizeOsgTexture(const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Geometry>& geom)
{
	try {
		osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
		osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
		if (texCoords.valid() && texture.valid())
		{
			osg::ref_ptr<osg::Image> image = texture->getImage();
			if (image.valid())
			{
				image = resizeImageToPowerOfTwo(image, _options.maxTextureWidth, _options.maxTextureHeight);
				// 缩放走的是私有副本：把副本挂回本 drawable 克隆出来的纹理上，
				// 否则后续图集/导出读到的还是源模型里那张原图。
				if (image != texture->getImage())
				{
					texture->setImage(image.get());
				}
				if (_options.packTexture)
				{
					bool bBuildTexturePacker = true;
					for (const auto& texCoord : *texCoords.get())
					{
						const float texCoordX = osg::absolute(texCoord.x());
						const float texCoordY = osg::absolute(texCoord.y());

						if (texCoordX > 1.0 || texCoordY > 1.0)
						{
							bBuildTexturePacker = false;
							break;
						}
					}
					if (bBuildTexturePacker)
					{
						if (image->s() <= _options.maxTextureWidth || image->t() <= _options.maxTextureHeight)
						{
							bool bAdd = true;
							for (osg::ref_ptr<osg::Image> img : _images)
							{
								if (img->getFileName() == image->getFileName())
								{
									_geometryImgMap[geom] = img;
									bAdd = false;
								}
							}
							if (bAdd)
							{
								_images.push_back(image);
								_geometryImgMap[geom] = image;
							}
						}
						else
						{
							exportOsgTextureIfNeeded(texture);
						}
					}
					else
					{
						exportOsgTextureIfNeeded(texture);
					}
				}
				else
				{
					exportOsgTextureIfNeeded(texture);
				}
			}
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::optimizeOsgTextureSize(osg::ref_ptr<osg::Texture2D> texture)
{
	try {
		if (texture.valid())
		{
			if (texture->getNumImages())
			{
				osg::ref_ptr<osg::Image> image = texture->getImage(0);
				if (image.valid())
				{
					image = resizeImageToPowerOfTwo(image, _options.maxTextureWidth, _options.maxTextureHeight);
					if (image != texture->getImage(0))
					{
						texture->setImage(0, image.get());
					}
				}
			}
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::exportOsgTextureIfNeeded(osg::ref_ptr<osg::Texture2D> texture, const GltfTextureType type)
{
	try {
		if (texture.valid())
		{
			if (texture->getNumImages())
			{
				osg::ref_ptr<osg::Image> image = texture->getImage();
				if (image.valid())
				{
					std::string fileFieldName;
					switch (type)
					{
					case GltfTextureType::NORMAL:
						fileFieldName = NORMAL_TEXTURE_FILENAME;
						break;
					case GltfTextureType::OCCLUSION:
						fileFieldName = OCCLUSION_TEXTURE_FILENAME;
						break;
					case GltfTextureType::EMISSIVE:
						fileFieldName = EMISSIVE_TEXTURE_FILENAME;
						break;
					case GltfTextureType::METALLICROUGHNESS:
						fileFieldName = MR_TEXTURE_FILENAME;
						break;
					case GltfTextureType::BASECOLOR:
						fileFieldName = BASECOLOR_TEXTURE_FILENAME;
						break;
					case GltfTextureType::DIFFUSE:
						fileFieldName = DIFFUSE_TEXTURE_FILENAME;
						break;
					case GltfTextureType::SPECULARGLOSSINESS:
						fileFieldName = SG_TEXTURE_FILENAME;
						break;
					default:
						break;
					}


					std::string name;
					texture->getUserValue(fileFieldName, name);
					if (name.empty())
					{
						const std::string fullPath = exportImage(image, type);
						texture->setUserValue(fileFieldName, fullPath);
					}
				}
			}
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

bool GltfOptimizer::TextureAtlasBuilderVisitor::gltfMaterialHasTexture(const osg::ref_ptr<GltfMaterial>& gltfMaterial)
{
	try {
		if (gltfMaterial->normalTexture.valid()) { return true; }
		if (gltfMaterial->occlusionTexture.valid()) { return true; }
		if (gltfMaterial->emissiveTexture.valid()) { return true; }

		osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
		if (gltfMRMaterial.valid())
		{
			if (gltfMRMaterial->metallicRoughnessTexture.valid()) { return true; }
			if (gltfMRMaterial->baseColorTexture.valid()) { return true; }
		}
		for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
		{
			GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
			if (auto* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension))
			{
				if (pbrSpecularGlossiness_extension->osgDiffuseTexture.valid()) { return true; }
				if (pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture.valid()) { return true; }
			}
		}
		return false;
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
		return false;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::optimizeOsgMaterial(const osg::ref_ptr<GltfMaterial>& gltfMaterial, const osg::ref_ptr<osg::Geometry>& geom)
{
	try {
		if (gltfMaterial.valid())
		{
			osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
			if (texCoords.valid())
			{
				optimizeOsgTextureSize(gltfMaterial->normalTexture);
				optimizeOsgTextureSize(gltfMaterial->occlusionTexture);
				optimizeOsgTextureSize(gltfMaterial->emissiveTexture);
				osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
				if (gltfMRMaterial.valid())
				{
					optimizeOsgTextureSize(gltfMRMaterial->metallicRoughnessTexture);
					optimizeOsgTextureSize(gltfMRMaterial->baseColorTexture);
				}
				for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
				{
					GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
					if (auto* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension))
					{
						optimizeOsgTextureSize(pbrSpecularGlossiness_extension->osgDiffuseTexture);
						optimizeOsgTextureSize(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
					}
				}

				if (_options.packTexture)
				{
					bool bBuildTexturePacker = true;
					for (const auto& texCoord : *texCoords.get())
					{
						const float texCoordX = osg::absolute(texCoord.x());
						const float texCoordY = osg::absolute(texCoord.y());

						if (texCoordX > 1.0 || texCoordY > 1.0)
						{
							bBuildTexturePacker = false;
							break;
						}
					}
					if (bBuildTexturePacker)
					{
						bool bAdd = true;
						for (GltfMaterial* item : _gltfMaterials)
						{
							if (item == gltfMaterial.get())
							{
								_geometryGltfMaterialMap[geom] = item;
								bAdd = false;
							}
						}
						if (bAdd)
						{
							_gltfMaterials.push_back(gltfMaterial.get());
							_geometryGltfMaterialMap[geom] = gltfMaterial.get();
						}
					}
					else
					{
						exportOsgTextureIfNeeded(gltfMaterial->normalTexture, GltfTextureType::NORMAL);
						exportOsgTextureIfNeeded(gltfMaterial->occlusionTexture, GltfTextureType::OCCLUSION);
						exportOsgTextureIfNeeded(gltfMaterial->emissiveTexture, GltfTextureType::EMISSIVE);
						osg::ref_ptr<GltfPbrMRMaterial> localGltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
						if (localGltfMRMaterial.valid())
						{
							exportOsgTextureIfNeeded(localGltfMRMaterial->metallicRoughnessTexture, GltfTextureType::METALLICROUGHNESS);
							exportOsgTextureIfNeeded(localGltfMRMaterial->baseColorTexture);
						}
						for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
						{
							GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
							if (auto* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension))
							{
								exportOsgTextureIfNeeded(pbrSpecularGlossiness_extension->osgDiffuseTexture, GltfTextureType::DIFFUSE);
								exportOsgTextureIfNeeded(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture, GltfTextureType::SPECULARGLOSSINESS);
							}
						}
					}
				}
				else
				{
					exportOsgTextureIfNeeded(gltfMaterial->normalTexture, GltfTextureType::NORMAL);
					exportOsgTextureIfNeeded(gltfMaterial->occlusionTexture, GltfTextureType::OCCLUSION);
					exportOsgTextureIfNeeded(gltfMaterial->emissiveTexture, GltfTextureType::EMISSIVE);
					osg::ref_ptr<GltfPbrMRMaterial> localGltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
					if (localGltfMRMaterial.valid())
					{
						exportOsgTextureIfNeeded(localGltfMRMaterial->metallicRoughnessTexture, GltfTextureType::METALLICROUGHNESS);
						exportOsgTextureIfNeeded(localGltfMRMaterial->baseColorTexture);
					}
					for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
					{
						GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
						if (auto* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension))
						{
							exportOsgTextureIfNeeded(pbrSpecularGlossiness_extension->osgDiffuseTexture, GltfTextureType::DIFFUSE);
							exportOsgTextureIfNeeded(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture, GltfTextureType::SPECULARGLOSSINESS);
						}
					}
				}
			}
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packOsgTextures()
{
	try {
		if (_images.empty()) { return; }
		while (_images.size())
		{
			TexturePacker packer(_options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
			std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
			osg::ref_ptr<osg::Image> packedImage = packImges(packer, _images, deleteImgs);

			if (packedImage.valid() && deleteImgs.size())
			{
				const double oldWidth = packedImage->s(), oldHeight = packedImage->t();
				packedImage = resizeImageToPowerOfTwo(packedImage, _options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
				const int width = packedImage->s(), height = packedImage->t();
				const double scaleWidth = width / oldWidth, sclaeHeight = height / oldHeight;
				packer.setScales(scaleWidth, sclaeHeight);
				const std::string fullPath = exportImage(packedImage, GltfTextureType::BASECOLOR);
				for (auto& entry : _geometryImgMap)
				{
					osg::Geometry* geometry = entry.first;
					osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
					osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

					osg::ref_ptr<osg::Image> image = entry.second;
					int w, h;
					double x, y;
					const size_t id = packer.getId(image);
					if (packer.getPackingData(id, x, y, w, h))
					{
						texture->setUserValue(TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME, true);
						double offsetX = x / width;
						texture->setUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_X, osg::clampTo(offsetX, 0.0, 1.0));
						double offsetY = y / height;
						texture->setUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_Y, osg::clampTo(offsetY, 0.0, 1.0));
						double scaleX = static_cast<double>(w) / width;
						texture->setUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_X, osg::clampTo(scaleX, 0.0, 1.0));
						double scaleY = static_cast<double>(h) / height;
						texture->setUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_Y, osg::clampTo(scaleY, 0.0, 1.0));
						texture->setUserValue(TEX_TRANSFORM_BASECOLOR_TEXCOORD, 0);
						texture->setUserValue(BASECOLOR_TEXTURE_FILENAME, fullPath);
					}
				}
				removePackedImages(_images, deleteImgs);
			}

			packedImage = nullptr;
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

osg::ref_ptr<osg::Image> GltfOptimizer::TextureAtlasBuilderVisitor::packImges(TexturePacker& packer, std::vector<osg::ref_ptr<osg::Image>>& imgs, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs)
{
	try {
		int area = _options.maxTextureAtlasWidth * _options.maxTextureAtlasHeight;
		std::sort(imgs.begin(), imgs.end(), GltfOptimizer::TextureAtlasBuilderVisitor::compareImageHeight);
		std::vector<size_t> indexPacks;
		osg::ref_ptr<osg::Image> packedImage;

		for (size_t i = 0; i < imgs.size(); ++i)
		{
			osg::ref_ptr<osg::Image> img = imgs.at(i);
			if (area >= img->s() * img->t())
			{
				if (deleteImgs.size())
				{
					if (deleteImgs.at(0)->getInternalTextureFormat() == img->getInternalTextureFormat() &&
						deleteImgs.at(0)->getPixelFormat() == img->getPixelFormat())
					{
						indexPacks.push_back(packer.addElement(img));
						area -= img->s() * img->t();
						deleteImgs.push_back(img);
					}
				}
				else
				{
					indexPacks.push_back(packer.addElement(img));
					area -= img->s() * img->t();
					deleteImgs.push_back(img);
				}
				if (i == (imgs.size() - 1))
				{
					packImages(packedImage, indexPacks, deleteImgs, packer);
				}
			}
			else
			{
				packImages(packedImage, indexPacks, deleteImgs, packer);
				break;
			}
		}

		return packedImage;
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
		return nullptr;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::updateGltfMaterialUserValue(
	osg::ref_ptr<GltfMaterial> gltfMaterial,
	osg::ref_ptr<osg::Texture2D> texture,
	const double offsetX,
	const double offsetY,
	const double scaleX,
	const double scaleY,
	const std::string fullPath,
	const GltfTextureType type)
{
	try {
		std::string textureFileFieldName, textureTransformExtensionName, textureOffsetXExtension, textureOffsetYExtension, textureScaleXExtension, textureScaleYExtension, textureTecCoord;
		switch (type)
		{
		case GltfTextureType::NORMAL:
			textureFileFieldName = NORMAL_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_NORMAL_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_NORMAL_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_NORMAL_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_NORMAL_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_NORMAL_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_NORMAL_TEXCOORD;
			break;
		case GltfTextureType::OCCLUSION:
			textureFileFieldName = OCCLUSION_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_OCCLUSION_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_OCCLUSION_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_OCCLUSION_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_OCCLUSION_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_OCCLUSION_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_OCCLUSION_TEXCOORD;
			break;
		case GltfTextureType::EMISSIVE:
			textureFileFieldName = EMISSIVE_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_EMISSIVE_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_EMISSIVE_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_EMISSIVE_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_EMISSIVE_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_EMISSIVE_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_EMISSIVE_TEXCOORD;
			break;
		case GltfTextureType::METALLICROUGHNESS:
			textureFileFieldName = MR_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_MR_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_MR_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_MR_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_MR_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_MR_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_MR_TEXCOORD;
			break;
		case GltfTextureType::BASECOLOR:
			textureFileFieldName = BASECOLOR_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_BASECOLOR_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_BASECOLOR_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_BASECOLOR_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_BASECOLOR_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_BASECOLOR_TEXCOORD;
			break;
		case GltfTextureType::DIFFUSE:
			textureFileFieldName = DIFFUSE_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_DIFFUSE_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_DIFFUSE_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_DIFFUSE_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_DIFFUSE_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_DIFFUSE_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_DIFFUSE_TEXCOORD;
			break;
		case GltfTextureType::SPECULARGLOSSINESS:
			textureFileFieldName = SG_TEXTURE_FILENAME;
			textureTransformExtensionName = TEX_TRANSFORM_SG_TEXTURE_NAME;
			textureOffsetXExtension = TEX_TRANSFORM_SG_OFFSET_X;
			textureOffsetYExtension = TEX_TRANSFORM_SG_OFFSET_Y;
			textureScaleXExtension = TEX_TRANSFORM_SG_SCALE_X;
			textureScaleYExtension = TEX_TRANSFORM_SG_SCALE_Y;
			textureTecCoord = TEX_TRANSFORM_SG_TEXCOORD;
			break;
		default:
			break;
		}

		gltfMaterial->setUserValue(textureTransformExtensionName, true);
		gltfMaterial->setUserValue(textureOffsetXExtension, osg::clampTo(offsetX, 0.0, 1.0));
		gltfMaterial->setUserValue(textureOffsetYExtension, osg::clampTo(offsetY, 0.0, 1.0));
		gltfMaterial->setUserValue(textureScaleXExtension, osg::clampTo(scaleX, 0.0, 1.0));
		gltfMaterial->setUserValue(textureScaleYExtension, osg::clampTo(scaleY, 0.0, 1.0));
		gltfMaterial->setUserValue(textureTecCoord, 0);
		texture->setUserValue(textureFileFieldName, fullPath);
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::removeRepeatImages(std::vector<osg::ref_ptr<osg::Image>>& imgs)
{
	try {
		if (!imgs.size()) { return; }
		// 用于存储唯一图像的集合
		std::unordered_set<std::string> fileNameSet; // 存储文件名的集合
		std::unordered_set<std::string> hashSet;     // 存储哈希值的集合
		std::vector<osg::ref_ptr<osg::Image>> uniqueImages; // 去重后的图像列表

		std::vector<osg::ref_ptr<osg::Image>> filenameEmptyImgs;
		std::vector<osg::ref_ptr<osg::Image>> filenameNotEmptyImgs;
		// 第一次遍历，检查是否所有文件名为空
		for (const auto& img : imgs)
		{
			if (img->getFileName().empty())
			{
				filenameEmptyImgs.push_back(img);
			}
			else
			{
				filenameNotEmptyImgs.push_back(img);
			}
		}

		for (const auto& img : filenameEmptyImgs)
		{
			std::string hash = TexturePacker::computeImageHash(img);
			if (hashSet.find(hash) == hashSet.end()) // 如果哈希值不重复
			{
				hashSet.insert(hash);
				uniqueImages.push_back(img);
			}
		}

		for (const auto& img : filenameNotEmptyImgs)
		{
			const std::string fileName = img->getFileName();
			if (fileNameSet.find(fileName) == fileNameSet.end()) // 如果文件名不重复
			{
				fileNameSet.insert(fileName);
				uniqueImages.push_back(img);
			}
		}

		// 用去重后的结果替换原始图像列表
		imgs = std::move(uniqueImages);
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::addImageFromTexture(const osg::ref_ptr<osg::Texture2D>& texture, std::vector<osg::ref_ptr<osg::Image>>& imgs)
{
	try {
		if (texture.valid())
		{
			if (texture->getNumImages())
			{
				bool bAdd = true;
				osg::ref_ptr<osg::Image> image = texture->getImage(0);
				for (osg::ref_ptr<osg::Image> img : imgs)
				{
					if (img == image || (img->getFileName() == image->getFileName() && !img->getFileName().empty()))
					{
						bAdd = false;
					}
				}
				if (bAdd)
				{
					image = resizeImageToPowerOfTwo(image, _options.maxTextureWidth, _options.maxTextureHeight);
					// 图集回填时按 texture->getImage() 取 packer id，所以缩放出来的
					// 私有副本必须挂回这张纹理，否则打包坐标对不上。
					if (image != texture->getImage(0))
					{
						texture->setImage(0, image.get());
					}
					imgs.push_back(image);
				}
			}
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::removePackedImages(std::vector<osg::ref_ptr<osg::Image>>& imgs, const std::vector<osg::ref_ptr<osg::Image>>& deleteImgs)
{
	for (osg::ref_ptr<osg::Image> img : deleteImgs)
	{
		auto it = std::remove(imgs.begin(), imgs.end(), img.get());
		imgs.erase(it, imgs.end());
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::processTextureImages(std::vector<osg::ref_ptr<osg::Image>>& images, std::unordered_map<osg::Geometry*, osg::ref_ptr<GltfMaterial>> geometryGltfMaterialMap, const GltfTextureType type, const std::function<osg::ref_ptr<osg::Texture2D>(GltfMaterial*)>& getTextureFunc)
{
	try {
		removeRepeatImages(images);

		// 诊断用：WB_ATLAS_TRACE=1 时把每轮进打包器的图像键序列和落盘页名打出来，
		// 用来判"A/B 两次的差别出在收图阶段还是装配阶段"。不参与任何决策。
		const bool atlasTrace = std::getenv("WB_ATLAS_TRACE") != nullptr;
		while (!images.empty()) {
			if (atlasTrace) {
				std::ostringstream oss;
				oss << "[atlas] type=" << static_cast<int>(type) << " n=" << images.size() << " keys=";
				for (size_t i = 0; i < images.size(); ++i) {
					if (i) { oss << ","; }
					// 全长键 + 内容哈希：只打 imageOrderKey 的前若干字符会把
					// "D:\BaiduNetdiskDownload\..." 这种公共前缀全部折叠成同一个字符串，
					// 从而把"输入不同"误判成"输入相同"。
					osg::Image* im = images[i].get();
					// name() 也打出来：TexturePacker::getId 用 name()+尺寸+格式做匹配，
					// 而 computeImageHash 不含 name()，两者不一致时页内容会跟着变。
					oss << imageOrderKey(im) << "|nm=" << im->getName()
					    << '|' << im->s() << 'x' << im->t()
					    << '|' << TexturePacker::computeImageHash(images[i]);
				}
				OSG_NOTICE << oss.str() << std::endl;
			}
			TexturePacker packer(_options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
			std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
			osg::ref_ptr<osg::Image> packedImage = packImges(packer, images, deleteImgs);
			if (atlasTrace) {
				OSG_NOTICE << "[atlas] packed=" << deleteImgs.size()
				           << " dims=" << (packedImage.valid() ? packedImage->s() : -1) << "x"
				           << (packedImage.valid() ? packedImage->t() : -1) << std::endl;
			}

			if (packedImage && !deleteImgs.empty()) {
				const double oldWidth = packedImage->s();
				const double oldHeight = packedImage->t();
				packedImage = resizeImageToPowerOfTwo(packedImage, _options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
				const int width = packedImage->s();
				const int height = packedImage->t();
				const double scaleWidth = width / oldWidth;
				const double scaleHeight = height / oldHeight;
				packer.setScales(scaleWidth, scaleHeight);
				const std::string fullPath = exportImage(packedImage, type);
				if (atlasTrace) { OSG_NOTICE << "[atlas] page -> " << fullPath << std::endl; }


				for (auto it = geometryGltfMaterialMap.begin(); it != geometryGltfMaterialMap.end();) {
					osg::Geometry* geometry = it->first;
					osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
					osg::ref_ptr<GltfMaterial> currentMaterial = dynamic_cast<GltfMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));

					if (currentMaterial) {
						osg::ref_ptr<osg::Texture2D> texture = getTextureFunc(currentMaterial.get());
						if (texture && texture->getNumImages()) {
							osg::ref_ptr<osg::Image> image = texture->getImage();
							const size_t id = packer.getId(image);
							int w, h;
							double x, y;
							if (packer.getPackingData(id, x, y, w, h)) {
								const double offsetX = x / width;
								const double offsetY = y / height;
								const double scaleX = static_cast<double>(w) / width;
								const double scaleY = static_cast<double>(h) / height;
								updateGltfMaterialUserValue(currentMaterial, texture, offsetX, offsetY, scaleX, scaleY, fullPath, type);
								it = geometryGltfMaterialMap.erase(it);
								continue;
							}
						}
					}
					++it;
				}

				removePackedImages(images, deleteImgs);
			}
			packedImage = nullptr;
		}
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::processGltfGeneralImages(std::vector<osg::ref_ptr<osg::Image>>& imgs, const GltfTextureType type)
{
	auto getTextureFunc = [type](GltfMaterial* material) -> osg::ref_ptr<osg::Texture2D> {
		switch (type) {
		case GltfTextureType::NORMAL:
			return material->normalTexture;
		case GltfTextureType::OCCLUSION:
			return material->occlusionTexture;
		case GltfTextureType::EMISSIVE:
			return material->emissiveTexture;
		default:
			return nullptr;
		}
		};

	processTextureImages(imgs, _geometryGltfMaterialMap, type, getTextureFunc);
}

void GltfOptimizer::TextureAtlasBuilderVisitor::processGltfPbrMRImages(std::vector<osg::ref_ptr<osg::Image>>& imageList, const GltfTextureType type)
{
	auto getTextureFunc = [type](GltfMaterial* material) -> osg::ref_ptr<osg::Texture2D> {
		auto gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(material);
		if (gltfMRMaterial) {
			switch (type) {
			case GltfTextureType::METALLICROUGHNESS:
				return gltfMRMaterial->metallicRoughnessTexture;
			case GltfTextureType::BASECOLOR:
				return gltfMRMaterial->baseColorTexture;
			default:
				return nullptr;
			}
		}
		return nullptr;
		};

	processTextureImages(imageList, _geometryGltfMaterialMap, type, getTextureFunc);
}

void GltfOptimizer::TextureAtlasBuilderVisitor::processGltfPbrSGImages(std::vector<osg::ref_ptr<osg::Image>>& images, const GltfTextureType type)
{
	auto getTextureFunc = [type](GltfMaterial* material) -> osg::ref_ptr<osg::Texture2D> {
		for (size_t j = 0; j < material->materialExtensionsByCesiumSupport.size(); ++j) {
			GltfExtension* extension = material->materialExtensionsByCesiumSupport.at(j);
			if (auto* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension)) {
				switch (type) {
				case GltfTextureType::DIFFUSE:
					return pbrSpecularGlossiness_extension->osgDiffuseTexture;
				case GltfTextureType::SPECULARGLOSSINESS:
					return pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture;
				default:
					return nullptr;
				}
			}
		}
		return nullptr;
		};

	processTextureImages(images, _geometryGltfMaterialMap, type, getTextureFunc);
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packOsgMaterials()
{
	try {
		if (_gltfMaterials.empty()) { return; }
		std::vector<osg::ref_ptr<osg::Image>> normalImgs, occlusionImgs, emissiveImgs, mrImgs, baseColorImgs, diffuseImgs, sgImgs;
		for (GltfMaterial* material : _gltfMaterials)
		{
			addImageFromTexture(material->normalTexture, normalImgs);
			addImageFromTexture(material->occlusionTexture, occlusionImgs);
			addImageFromTexture(material->emissiveTexture, emissiveImgs);

			osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(material);
			if (gltfMRMaterial.valid())
			{
				addImageFromTexture(gltfMRMaterial->metallicRoughnessTexture, mrImgs);
				addImageFromTexture(gltfMRMaterial->baseColorTexture, baseColorImgs);
			}
			for (size_t i = 0; i < material->materialExtensionsByCesiumSupport.size(); ++i)
			{
				GltfExtension* extension = material->materialExtensionsByCesiumSupport.at(i);
				if (auto* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension))
				{
					addImageFromTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture, diffuseImgs);
					addImageFromTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture, sgImgs);
				}
			}
		}

		processGltfGeneralImages(normalImgs, GltfTextureType::NORMAL);
		processGltfGeneralImages(occlusionImgs, GltfTextureType::OCCLUSION);
		processGltfGeneralImages(emissiveImgs, GltfTextureType::EMISSIVE);

		processGltfPbrMRImages(mrImgs, GltfTextureType::METALLICROUGHNESS);
		processGltfPbrMRImages(baseColorImgs, GltfTextureType::BASECOLOR);

		processGltfPbrSGImages(diffuseImgs, GltfTextureType::DIFFUSE);
		processGltfPbrSGImages(sgImgs, GltfTextureType::SPECULARGLOSSINESS);

		normalImgs.clear();
		occlusionImgs.clear();
		emissiveImgs.clear();
		mrImgs.clear();
		baseColorImgs.clear();
		diffuseImgs.clear();
		sgImgs.clear();
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer)
{
	size_t numImages;
	img = packer.pack(numImages, true);
	if (!img.valid() || numImages != indexes.size())
	{
		packer.removeElement(indexes.back());
		indexes.pop_back();
		deleteImgs.pop_back();
		packImages(img, indexes, deleteImgs, packer);
	}
}

osg::ref_ptr<osg::Image> GltfOptimizer::TextureAtlasBuilderVisitor::resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img, const int maxWidth, const int maxHeight)
{
	try {
		if (!img.valid())
		{
			return img;
		}
		int originalWidth = -1;
		int originalHeight = -1;
		img->getUserValue(ORIGIN_WIDTH, originalWidth);
		img->getUserValue(ORIGIN_HEIGHT, originalHeight);
		if (originalWidth == -1 || originalHeight == -1)
		{
			originalWidth = img->s();
			originalHeight = img->t();
		}
		int newWidth = osg::Image::computeNearestPowerOfTwo(originalWidth);
		int newHeight = osg::Image::computeNearestPowerOfTwo(originalHeight);

		newWidth = newWidth > maxWidth ? maxWidth : newWidth;
		newHeight = newHeight > maxHeight ? maxHeight : newHeight;
		if (newWidth == originalWidth && newHeight == originalHeight)
		{
			// 不需要动像素，也就不动这张图：连 ORIGIN_* 都不往上面写。
			// 写用户值同样是改共享对象，会让后续瓦片看到"来自别处的原始尺寸"。
			return img;
		}
		// 必须私有化后再缩放：源模型里同一张贴图会被多个瓦片/多个 drawable 共用，
		// 瓦片克隆 StateSet 只克隆到 osg::Texture 层（osg::Image 依旧共享，实测），
		// 就地 scaleImage 等于让第一个降分辨率的瓦片改掉它后面所有瓦片的贴图。
		osg::ref_ptr<osg::Image> copy = new osg::Image(*img.get(), osg::CopyOp::SHALLOW_COPY);
		copy->setUserValue(ORIGIN_WIDTH, originalWidth);
		copy->setUserValue(ORIGIN_HEIGHT, originalHeight);
		copy->scaleImage(newWidth, newHeight, copy->r());
		return copy;
	}
	catch (const std::exception& e) {
		OSG_NOTICE << e.what() << std::endl;
		return img;
	}
}

std::string GltfOptimizer::TextureAtlasBuilderVisitor::imageOrderKey(osg::Image* img)
{
	if (!img) { return std::string(); }
	if (!img->getFileName().empty()) { return "f:" + img->getFileName(); }
	const char* const cacheKey = "osgGIS_atlasOrderKey";
	std::string cached;
	if (img->getUserValue<std::string>(cacheKey, cached)) { return cached; }
	cached = "h:" + TexturePacker::computeImageHash(osg::ref_ptr<osg::Image>(img));
	img->setUserValue(cacheKey, cached);
	return cached;
}

bool GltfOptimizer::TextureAtlasBuilderVisitor::compareImageHeight(osg::ref_ptr<osg::Image> img1, osg::ref_ptr<osg::Image> img2)
{
	if (img1->t() != img2->t())
	{
		return img1->t() > img2->t();
	}
	if (img1->s() != img2->s())
	{
		return img1->s() > img2->s();
	}
	// 同尺寸在这里是常态，而 packImges 用的是不保序的 std::sort：只比尺寸时，并列组内
	// 谁先落地由进图顺序（= 场景图遍历序 / 地址序）决定 ⇒ 子图位置变 ⇒ 整页内容哈希变
	// ⇒ 图集页名换、引用它的瓦片字节跟着换，同二进制两次跑不逐位相同（A-52）。
	// 补一条纯内容派生的末级键，让顺序只由内容决定。
	return imageOrderKey(img1.get()) < imageOrderKey(img2.get());
}

/** MergeTransformVisitor */
void GltfOptimizer::MergeTransformVisitor::apply(osg::MatrixTransform& xtransform)
{
	osg::Matrixd previousMatrix = _currentMatrix;
	osg::Matrixd localMatrix;
	xtransform.computeLocalToWorldMatrix(localMatrix, this);
	_currentMatrix.preMult(localMatrix);
	traverse(xtransform);
	_currentMatrix = previousMatrix;
}

void GltfOptimizer::MergeTransformVisitor::apply(osg::Geode& geode)
{
	_matrixNodesMap[_currentMatrix].push_back(&geode);
}

osg::Node* GltfOptimizer::MergeTransformVisitor::getNode()
{
	osg::ref_ptr<osg::Group> group = new osg::Group;

	for (auto& item : _matrixNodesMap)
	{
		osg::Matrixd matrix = item.first;
		osg::ref_ptr<osg::MatrixTransform> transformNode = new osg::MatrixTransform;
		transformNode->setMatrix(matrix);
		for (osg::ref_ptr<osg::Node> node : item.second)
		{
			transformNode->addChild(node);
		}
		group->addChild(transformNode);
	}

	return group.release();
}
