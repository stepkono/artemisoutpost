// Fill out your copyright notice in the Description page of Project Settings.

#include "CutoutTileExcluder.h"

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <CesiumGeometry/CullingResult.h>
#include <CesiumGeometry/OrientedBoundingBox.h>
#include <glm/geometric.hpp>

FCutoutTileExcluder::FCutoutTileExcluder(const CesiumGeospatial::Ellipsoid& InEllipsoid)
	: Ellipsoid(InEllipsoid)
{
}

bool FCutoutTileExcluder::SetCutout(const glm::dvec3 (&Corners)[4])
{
	const glm::dvec3 Center = (Corners[0] + Corners[1] + Corners[2] + Corners[3]) * 0.25;

	glm::dvec3 Normals[4];
	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		// The plane through the moon center (origin) and one cutout edge.
		const glm::dvec3 Normal = glm::cross(Corners[Edge], Corners[(Edge + 1) % 4]);
		const double Length = glm::length(Normal);
		if (Length <= 0.0)
		{
			bHasCutout = false;
			return false;
		}
		// Independent of the corner winding: the normal has to point towards the cutout center.
		Normals[Edge] = glm::dot(Normal, Center) < 0.0 ? -Normal / Length : Normal / Length;
	}

	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		SidePlanes[Edge] = CesiumGeometry::Plane(glm::dvec3(0.0), Normals[Edge]);
	}
	bHasCutout = true;
	return true;
}

bool FCutoutTileExcluder::shouldExclude(const Cesium3DTilesSelection::Tile& Tile) const noexcept
{
	if (!bEnabled || !bHasCutout)
	{
		return false;
	}

	const CesiumGeometry::OrientedBoundingBox Box =
		Cesium3DTilesSelection::getOrientedBoundingBoxFromBoundingVolume(Tile.getBoundingVolume(), Ellipsoid);

	// Completely behind one side plane = completely outside the pyramid. Tiles crossing the cutout edge stay.
	for (const CesiumGeometry::Plane& Plane : SidePlanes)
	{
		if (Box.intersectPlane(Plane) == CesiumGeometry::CullingResult::Outside)
		{
			return true;
		}
	}
	return false;
}
