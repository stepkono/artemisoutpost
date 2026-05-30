// Fill out your copyright notice in the Description page of Project Settings.


#include "GeoUtils.h"

UGeoUtils::UGeoUtils()
{
}

UGeoUtils::~UGeoUtils()
{
}

FVector UGeoUtils::GetUpVector(FVector AtCoordsWithHeight, ACesiumGeoreference* CesiumGeoreference)
{
	UCesiumEllipsoid* Ellipsoid = CesiumGeoreference->GetEllipsoid();

	// We have to transform the lon-lat-coords to ECEF reference frame to get the surface normal at these coords
	// Here ECEF uses the IAU2015_Moon reference system 
	const FVector ECEFCoords = Ellipsoid->LongitudeLatitudeHeightToEllipsoidCenteredEllipsoidFixed(AtCoordsWithHeight);
	
	const FVector ECEFSurfaceNormal = Ellipsoid->GeodeticSurfaceNormal(ECEFCoords);

	// Transform normal from ECEF to unreal world 
	const FMatrix ecefToUnreal = CesiumGeoreference->ComputeEarthCenteredEarthFixedToUnrealTransformation();
	const FVector UnrealSurfaceNormal = ecefToUnreal.TransformFVector4(FVector4(ECEFSurfaceNormal.X, ECEFSurfaceNormal.Y, ECEFSurfaceNormal.Z, 0.0)).GetSafeNormal();
	
	return UnrealSurfaceNormal;
}

FMatrix UGeoUtils::GetLocalSpatialReferenceFrame(const FVector &LonLatHeightPos, ACesiumGeoreference* CesiumGeoreference)
{
	// We build local cartesian reference frame at given location
	// We use Unreal's left-handed coordinate system as reference,
	// Where th Z-Axis points up and the Y-Axis points to right ->
	// in case of our local reference frame on the moon surface that will make north-east-up for (x, y, z) 
	const FVector Up = GetUpVector(LonLatHeightPos, CesiumGeoreference);
	const FVector East = FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal();
	const FVector North = FVector::CrossProduct(East, Up).GetSafeNormal();

	return FMatrix(North, East, Up, FVector::ZeroVector);
}

FQuat UGeoUtils::BuildQuatFromMatrix(FMatrix RotationMatrix)
{
	// Remove translation
	RotationMatrix.SetOrigin(FVector::ZeroVector);
	
	// UE_LOG(LogTemp, Warning, TEXT("Determinant RotM: %f"), RotationMatrix.Determinant());
	return FQuat(RotationMatrix);
}

FMatrix UGeoUtils::BuildMatrixFromVectors(FVector AxisX, FVector AxisY)
{
	const FVector NormalX = AxisX.GetSafeNormal();
	FVector NormalY = AxisY.GetSafeNormal();
	const FVector NormalZ = FVector::CrossProduct(NormalX, NormalY).GetSafeNormal();

	// The angle between x and y might be slightly off, so we ensure perfect orthogonality
	NormalY = FVector::CrossProduct(NormalZ, NormalX);
	
	const FMatrix M(
		FPlane(NormalX, 0.f),
		FPlane(NormalY, 0.f),
		FPlane(NormalZ, 0.f),
		FPlane(0.f, 0.f, 0.f, 1.f)
	);

	// UE_LOG(LogTemp, Warning, TEXT("Determinant Frame: %f"), M.Determinant());

	return M;
}

FMatrix UGeoUtils::CalculateRotationMatrix(FMatrix SourceMatrix, FMatrix TargetMatrix)
{
	return  SourceMatrix.Inverse() * TargetMatrix;
}


FCalibratedData UGeoUtils::CalibrateAnchors(const FVector& AAnchorPos, const FVector& BAnchorPos, const FVector& DAnchorPos)
{
	FCalibratedData CalibratedData = {};
	
	// Flatten edge vectors onto the horizontal plane
	const FVector AB = FVector(BAnchorPos.X - AAnchorPos.X, BAnchorPos.Y - AAnchorPos.Y, 0.f);
	FVector AD = FVector(DAnchorPos.X - AAnchorPos.X, DAnchorPos.Y - AAnchorPos.Y, 0.f);

	// Gram-Schmidt: force AD perpendicular to AB
	// Projects out the AB component from AD, leaving only the orthogonal part
	const FVector ABNorm = AB.GetSafeNormal();
	AD = AD - FVector::DotProduct(AD, ABNorm) * ABNorm;

	// Reconstruct clean corners: A stays fixed, B/D/C derived from orthogonal edges
	const FVector A = AAnchorPos;
	const FVector B = AAnchorPos + AB;
	const FVector C = AAnchorPos + AD;
	const FVector D = AAnchorPos + AB + AD;
	
	const FVector TableCenter = (A + B + C + D) * 0.25;
	const FVector TableNormal = FVector::CrossProduct(B - A, D - A).GetSafeNormal(); 
	
	CalibratedData.AAnchorPos = A; 
	CalibratedData.BAnchorPos = B;
	CalibratedData.CAnchorPos = C;
	CalibratedData.DAnchorPos = D;
	CalibratedData.PlaneCenter = TableCenter;
	CalibratedData.PlaneNormal = TableNormal;
	
	UE_LOG(LogTemp, Warning, TEXT("CalibrateAnchors: Table center: %s"), *TableCenter.ToString());
	UE_LOG(LogTemp, Warning, TEXT("CalibrateAnchors: Table normal: %s"), *TableNormal.ToString());
	
	return CalibratedData;
}
