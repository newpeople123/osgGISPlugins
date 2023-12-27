#pragma once
#ifndef OSG_GIS_PLUGINS_GEOMETRICERROR_H
#define OSG_GIS_PLUGINS_GEOMETRICERROR_H 1
#include "TileNode.h"
constexpr double InitialPixelSize = 25.0;
constexpr double DetailPixelSize = InitialPixelSize * 5;
constexpr double CesiumCanvasClientWidth = 1920;
constexpr double CesiumCanvasClientHeight = 931;
constexpr double CesiumFrustumAspectRatio = CesiumCanvasClientWidth / CesiumCanvasClientHeight;
const double CesiumFrustumFov = osg::PI / 3;
const double CesiumFrustumFovy = CesiumFrustumAspectRatio <= 1 ? CesiumFrustumFov : atan(tan(CesiumFrustumFov * 0.5) / CesiumFrustumAspectRatio) * 2.0;
constexpr double CesiumFrustumNear = 0.1;
constexpr double CesiumFrustumFar = 10000000000.0;
constexpr double CesiumCanvasViewportWidth = CesiumCanvasClientWidth;
constexpr double CesiumCanvasViewportHeight = CesiumCanvasClientHeight;
const double CesiumSSEDenominator = 2.0 * tan(0.5 * CesiumFrustumFovy);
constexpr double CesiumMaxScreenSpaceError = 16.0;

inline int GetPixelSize(const double& distance, const double& radius) {

	const double angularSize = 2.0 * atan(radius / distance);
	const double dpp = osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight;
	const int pixelSize = angularSize / dpp;
	return pixelSize;
}

/// <summary>
/// Calculate the distance between the model and the viewpoint based on the pixel size
/// </summary>
/// <param name="radius"></param>
/// <param name="pixelSize"></param>
/// <returns></returns>
inline double GetDistance(const double& radius, const double& pixelSize) {

	const double dpp = osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight;
	const double angularSize = dpp * pixelSize;

	const double distance = radius / tan(angularSize / 2);
	return distance;
}

inline double GetUpperGeometricError(const osg::ref_ptr<TileNode>& node) {

	double geometricError = 0.0;
	double distance = 0.0;
	double radius = 0.0;
	radius = node->nodes->getBound().radius();
	if (node->level == 0)
		distance = GetDistance(radius, InitialPixelSize);
	else
		distance = GetDistance(radius, DetailPixelSize);
	geometricError = distance * CesiumSSEDenominator * CesiumMaxScreenSpaceError / CesiumCanvasClientHeight;
	return geometricError;
}

inline double GetLowerGeometricError(const osg::ref_ptr<TileNode>& node) {

	double geometricError = 0.0;
	double distance = 0.0;
	double radius = 0.0;

	const int num = node->children->getNumChildren();
	if (num > 0) {
		for (int i = 0; i < num; ++i) {
			osg::ref_ptr<osg::Node> child = node->children->getChild(i);
			const TileNode* childNode = dynamic_cast<TileNode*>(child.get());
			const double childRadius = childNode->nodes->getBound().radius();
			if (childRadius > radius) {
				radius = childRadius;
			}
		}
	}
	else {
		return geometricError;
	}
	if (node->level == 0)
		distance = GetDistance(radius, InitialPixelSize);
	else
		distance = GetDistance(radius, DetailPixelSize);
	geometricError = distance * CesiumSSEDenominator * CesiumMaxScreenSpaceError / CesiumCanvasClientHeight;
	return geometricError;
}

inline double GetDistanceByGeometricError(double geometricError) {
	return geometricError / (CesiumSSEDenominator * CesiumMaxScreenSpaceError / CesiumCanvasClientHeight);
}

inline void GetTileNodeGeometricErrors(osg::ref_ptr<TileNode>& node) {
	node->upperGeometricError = GetUpperGeometricError(node);
	node->lowerGeometricError = GetLowerGeometricError(node);
	//const int num = node->children->getNumChildren();
	//for (int i = 0; i < num; ++i) {
	//	osg::ref_ptr<TileNode> childNode = dynamic_cast<TileNode*>(node->children->getChild(i));
	//	GetTileNodeGeometricErrors(childNode);
	//}
}
#endif // !OSG_GIS_PLUGINS_GEOMETRICERROR_H
