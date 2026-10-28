// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <Cesium3DTilesSelection/ITileExcluder.h>
#include <CesiumGeometry/Plane.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <glm/vec3.hpp>

/**
 * Excludes AR moon tiles that lie completely outside the table cutout, so they are neither loaded nor rendered.
 * The cutout is a pyramid with its apex at the moon center and its four sides through the cutout corners: the same
 * radial projection as longitude/latitude, without its limits at the antimeridian and the poles.
 *
 * Registered directly in the native tileset's excluders, no raster overlay involved. Cesium asks it for every tile
 * during tile selection, every frame, on the game thread.
 */
class FCutoutTileExcluder : public Cesium3DTilesSelection::ITileExcluder
{
public:
	explicit FCutoutTileExcluder(const CesiumGeospatial::Ellipsoid& InEllipsoid);

	// Corners in the tileset's earth-centered frame (meters), in loop order. Returns false for a degenerate cutout,
	// which then excludes nothing.
	bool SetCutout(const glm::dvec3 (&Corners)[4]);

	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	virtual bool shouldExclude(const Cesium3DTilesSelection::Tile& Tile) const noexcept override;

private:
	// Needed to turn bounding regions (longitude/latitude boxes) into oriented boxes.
	CesiumGeospatial::Ellipsoid Ellipsoid;

	// Side planes through the moon center, normals pointing into the cutout.
	CesiumGeometry::Plane SidePlanes[4];

	bool bHasCutout = false;
	bool bEnabled = true;
};
