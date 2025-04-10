#include "utils/ObbVisitor.h"
#include <osg/Geode>
using namespace osgGISPlugins;

void OBBVisitor::apply(osg::Group& group)
{
	// 备份当前矩阵
	const osg::Matrix previousMatrix = _currentMatrix;

	// 如果是Transform节点，累积变换矩阵
	if (const osg::Transform* transform = group.asTransform())
	{
		osg::Matrix localMatrix;
		transform->computeLocalToWorldMatrix(localMatrix, this);
		_currentMatrix.preMult(localMatrix);
	}

	// 继续遍历子节点
	traverse(group);

	// 恢复到之前的矩阵状态
	_currentMatrix = previousMatrix;
}

void OBBVisitor::apply(osg::Geode& geode)
{
	for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
	{
		osg::Geometry* geom = dynamic_cast<osg::Geometry*>(geode.getDrawable(i));
		if (geom)
		{
			osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
			if (vertices)
			{
				for (size_t i = 0; i < vertices->size(); ++i)
				{
					_vertices->push_back(vertices->at(i) * _currentMatrix);
				}
			}
		}
	}
	traverse(geode);
}

void OBBVisitor::calculateOBB()
{
	const int num = _vertices->size();
	osg::Matrix matTransform = getOBBOrientation(_vertices);

	matTransform = transpose(matTransform);
	//matTransform.inverse(matTransform);

	osg::Vec3 vecMax = matTransform * osg::Vec3(_vertices->at(0).x(), _vertices->at(0).y(), _vertices->at(0).z());

	osg::Vec3 vecMin = vecMax;

	for (int i = 1; i < num; i++)
	{
		osg::Vec3 vect = matTransform * osg::Vec3(_vertices->at(i).x(), _vertices->at(i).y(), _vertices->at(i).z());

		vecMax.x() = vecMax.x() > vect.x() ? vecMax.x() : vect.x();
		vecMax.y() = vecMax.y() > vect.y() ? vecMax.y() : vect.y();
		vecMax.z() = vecMax.z() > vect.z() ? vecMax.z() : vect.z();

		vecMin.x() = vecMin.x() < vect.x() ? vecMin.x() : vect.x();
		vecMin.y() = vecMin.y() < vect.y() ? vecMin.y() : vect.y();
		vecMin.z() = vecMin.z() < vect.z() ? vecMin.z() : vect.z();
	}

	osg::Matrix mattemp = transpose(matTransform);
	//matTransform.inverse(matTransform);

	_xAxis = osg::Vec3(mattemp(0, 0), mattemp(1, 0), mattemp(2, 0));
	_yAxis = osg::Vec3(mattemp(0, 1), mattemp(1, 1), mattemp(2, 1));
	_zAxis = osg::Vec3(mattemp(0, 2), mattemp(1, 2), mattemp(2, 2));

	_center = (vecMax + vecMin) / 2;
	matTransform.inverse(matTransform);
	_center = _center * matTransform;

	_xAxis.normalize();
	_yAxis.normalize();
	_zAxis.normalize();

	_extents = (vecMax - vecMin) / 2;

	computeExtAxis();
}

void OBBVisitor::computeExtAxis()
{
	_extentX = _xAxis * _extents.x();
	_extentY = _yAxis * _extents.y();
	_extentZ = _zAxis * _extents.z();
}

osg::Matrix OBBVisitor::getOBBOrientation(const osg::ref_ptr<osg::Vec3Array>& vertices)
{
	osg::Matrix result;

	if (vertices->size() <= 0)
		return osg::Matrix::identity();

	result = getConvarianceMatrix(vertices);

	// now get eigenvectors
	osg::Matrix evecs;
	osg::Vec3 evals;
	getEigenVectors(&evecs, &evals, result);

	evecs = transpose(evecs);
	//evecs.transpose();

	return evecs;
}

osg::Matrix OBBVisitor::getConvarianceMatrix(const osg::ref_ptr<osg::Vec3Array>& vertices)
{
	int i;
	osg::Matrix result;

	double S1[3];
	double S2[3][3];

	S1[0] = S1[1] = S1[2] = 0.0;
	S2[0][0] = S2[1][0] = S2[2][0] = 0.0;
	S2[0][1] = S2[1][1] = S2[2][1] = 0.0;
	S2[0][2] = S2[1][2] = S2[2][2] = 0.0;

	// get center of mass
	const int vertCount = vertices->size();
	for (i = 0; i < vertCount; i++)
	{
		S1[0] += (*vertices)[i].x();
		S1[1] += (*vertices)[i].y();
		S1[2] += (*vertices)[i].z();

		S2[0][0] += (*vertices)[i].x() * (*vertices)[i].x();
		S2[1][1] += (*vertices)[i].y() * (*vertices)[i].y();
		S2[2][2] += (*vertices)[i].z() * (*vertices)[i].z();
		S2[0][1] += (*vertices)[i].x() * (*vertices)[i].y();
		S2[0][2] += (*vertices)[i].x() * (*vertices)[i].z();
		S2[1][2] += (*vertices)[i].y() * (*vertices)[i].z();
	}

	const float n = (float)vertCount;
	// now get covariances
	result(0, 0) = (float)(S2[0][0] - S1[0] * S1[0] / n) / n;
	result(1, 1) = (float)(S2[1][1] - S1[1] * S1[1] / n) / n;
	result(2, 2) = (float)(S2[2][2] - S1[2] * S1[2] / n) / n;
	result(0, 1) = (float)(S2[0][1] - S1[0] * S1[1] / n) / n;
	result(1, 2) = (float)(S2[1][2] - S1[1] * S1[2] / n) / n;
	result(0, 2) = (float)(S2[0][2] - S1[0] * S1[2] / n) / n;
	result(1, 0) = result(0, 1);
	result(2, 0) = result(0, 2);
	result(2, 1) = result(1, 2);

	return result;
}

void OBBVisitor::getEigenVectors(osg::Matrix* vout, osg::Vec3* dout, osg::Matrix a)
{
	const int n = 3;
	int j, iq, ip, i;
	double tresh, theta, tau, t, sm, s, h, g, c;
	int nrot;
	osg::Vec3 b;
	osg::Vec3 z;
	osg::Matrix v;
	osg::Vec3 d;

	v = osg::Matrix::identity();
	for (ip = 0; ip < n; ip++)
	{
		getElement(b, ip) = a(ip, ip);// a.m[ip + 4 * ip];
		getElement(d, ip) = a(ip, ip);;// a.m[ip + 4 * ip];
		getElement(z, ip) = 0.0;
	}

	nrot = 0;

	for (i = 0; i < 50; i++)
	{
		sm = 0.0;
		for (ip = 0; ip < n; ip++) for (iq = ip + 1; iq < n; iq++) sm += fabs((a(ip, iq)));
		if (false/*fabs(sm) < FLT_EPSILON*/)
		{
			v = transpose(v);
			//v.transpose();
			*vout = v;
			*dout = d;
			return;
		}
		if (i < 3)
			tresh = 0.2 * sm / (n * n);
		else
			tresh = 0.0;

		for (ip = 0; ip < n; ip++)
		{
			for (iq = ip + 1; iq < n; iq++)
			{
				g = 100.0 * fabs(a(ip, iq)/*a.m[ip + iq * 4]*/);
				const float dmip = getElement(d, ip);
				const float dmiq = getElement(d, iq);

				if (i > 3 && fabs(dmip) + g == fabs(dmip) && fabs(dmiq) + g == fabs(dmiq))
				{
					/*a.m[ip + 4 * iq]*/ a(ip, iq) = 0.0;
				}
				else if (fabs(a(ip, iq)) > tresh)
				{
					h = dmiq - dmip;
					if (fabs(h) + g == fabs(h))
					{
						t = (a(ip, iq)) / h;
					}
					else
					{
						theta = 0.5 * h / (a(ip, iq));
						t = 1.0 / (fabs(theta) + sqrt(1.0 + theta * theta));
						if (theta < 0.0) t = -t;
					}
					c = 1.0 / sqrt(1 + t * t);
					s = t * c;
					tau = s / (1.0 + c);
					h = t * a(ip, iq);
					getElement(z, ip) -= (float)h;
					getElement(z, iq) += (float)h;
					getElement(d, ip) -= (float)h;
					getElement(d, iq) += (float)h;
					a(ip, iq) = 0.0;
					for (j = 0; j < ip; j++) { ROTATE(a, j, ip, j, iq); }
					for (j = ip + 1; j < iq; j++) { ROTATE(a, ip, j, j, iq); }
					for (j = iq + 1; j < n; j++) { ROTATE(a, ip, j, iq, j); }
					for (j = 0; j < n; j++) { ROTATE(v, j, ip, j, iq); }
					nrot++;
				}
			}
		}

		for (ip = 0; ip < n; ip++)
		{
			getElement(b, ip) += getElement(z, ip);
			getElement(d, ip) = getElement(b, ip);
			getElement(z, ip) = 0.0f;
		}
	}

	v = transpose(v);
	//v.transpose();
	*vout = v;
	*dout = d;
}

float& OBBVisitor::getElement(osg::Vec3& point, const int index)
{
	if (index == 0)
		return point.x();
	if (index == 1)
		return point.y();
	if (index == 2)
		return point.z();

	return point.x();
}

osg::Matrix OBBVisitor::transpose(const osg::Matrix& b)
{
	osg::Matrix mat;
	//mat.inverse(b);
	//mat.invert(b);
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			mat(i, j) = b(j, i);
		}
	}
	return mat;
}
