// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "SecureHash.h"
#include "ModuleInterface.h"

/** Helper struct for tracking dependency info for package timestamps */
class FPackageDependencyTrackingInfo
{
public:
	/** Full path name of the package */
	FString PackageName;
	/** The GUID of the source package */
	FGuid PackageGuid;
	/** Timestamp of the package (source not cooked) */
	FDateTime TimeStamp;
	/** Timestamp of the cooked package (not that actual cooked package - but the 'newest' of any dependencies) */
	FDateTime DependentTimeStamp;
	/** Does the package contain a map? */
	bool bContainsMap;
	/** Does the package contain shaders? (ie any material interface?) */
	bool bContainsShaders;
	/** Does the package contain blueprints? */
	bool bContainsBlueprints;
	/** Hash of the file */
	FMD5Hash FullPackageHash;
	/** Hash of the hash of this file, hashed with the hashes of dependent files */
	FMD5Hash DependentHash;
	/** Packages this package depends on */
	TMap<FString,FPackageDependencyTrackingInfo*> DependentPackages;

	/**
	 * has this package dependency tracking info been initialized
	 */
	bool bInitializedDependencies;
	bool bInitializedHashes;
	bool bValid;


	/** 
	 *	Temporary value indicating that this info is being processed...
	 *	Used to find circular references when determining dependency chains.
	 */
	bool bBeingProcessed;

	/**
	 *	Temporary value indicating that this info had a circular reference.
	 *	This indicates that the package needs to be 'post processed' to determine
	 *	the proper dependent time stamp.
	 */
	bool bHasCircularReferences;

	FPackageDependencyTrackingInfo()
		: DependentTimeStamp(FDateTime::MinValue())
		, bContainsMap(false)
		, bContainsShaders(false)
		, bContainsBlueprints(false)
		, bInitializedDependencies(false)
		, bInitializedHashes(false)
		, bValid(true)
		, bBeingProcessed(false)
		, bHasCircularReferences(false)
	{
	}

	FPackageDependencyTrackingInfo(const FString& InPackageName, const FDateTime& InTimeStamp)
		: PackageName(InPackageName)
		, TimeStamp(InTimeStamp)
		, DependentTimeStamp(FDateTime::MinValue())
		, bContainsMap(false)
		, bContainsShaders(false)
		, bContainsBlueprints(false)
		, bInitializedDependencies(false)
		, bInitializedHashes(false)
		, bValid(true)
		, bBeingProcessed(false)
		, bHasCircularReferences(false)
	{
	}

	FPackageDependencyTrackingInfo& operator=(const FPackageDependencyTrackingInfo& InInfo)
	{
		PackageName = InInfo.PackageName;
		PackageGuid = InInfo.PackageGuid;
		TimeStamp = InInfo.TimeStamp;
		DependentTimeStamp = InInfo.DependentTimeStamp;
		bContainsMap = InInfo.bContainsMap;
		bContainsShaders = InInfo.bContainsShaders;
		bContainsBlueprints = InInfo.bContainsBlueprints;
		DependentPackages = InInfo.DependentPackages;
		bInitializedDependencies = InInfo.bInitializedDependencies;
		bInitializedHashes = InInfo.bInitializedHashes;
		bValid = InInfo.bValid;
		return *this;
	}

	bool operator==(const FPackageDependencyTrackingInfo& InInfo) const
	{
		if ((PackageName != InInfo.PackageName) ||
			(PackageGuid != InInfo.PackageGuid) ||
			(TimeStamp != InInfo.TimeStamp) ||
			(DependentTimeStamp != InInfo.DependentTimeStamp) || 
			(bContainsMap != InInfo.bContainsMap) || 
			(bContainsShaders != InInfo.bContainsShaders) ||
			(bContainsBlueprints != InInfo.bContainsBlueprints) || 
			(DependentPackages.Num() != InInfo.DependentPackages.Num()))
		{
			return false;
		}

		// Check the actual contents of the dependent package map...
		for (TMap<FString,FPackageDependencyTrackingInfo*>::TConstIterator PkgIt(DependentPackages); PkgIt; ++PkgIt)
		{
			const FString& PkgName = PkgIt.Key();
			const FPackageDependencyTrackingInfo* PkgValue = PkgIt.Value();

			FPackageDependencyTrackingInfo*const * pCompareValue = InInfo.DependentPackages.Find(PkgName);
			if (pCompareValue == NULL)
			{
				// not found - not equal
				return false;
			}

			if (*pCompareValue != PkgValue)
			{
				// different value - not equal
				return false;
			}
		}

		return true;
	}
};

/**
 *	The module interface
 */
class FPackageDependencyInfoModule : public IModuleInterface
{
public:
	/** */
	virtual void StartupModule();

	/**
	 * Called before the plugin is unloaded, right before the plugin object is destroyed.
	 */
	virtual void ShutdownModule();

	/**
	 *	Determine the given packages dependent time stamp
	 *
	 *	@param	InPackageName		The package to process
	 *	@param	OutNewestTime		The dependent time stamp for the package.
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool DeterminePackageDependentTimeStamp(const TCHAR* InPackageName, FDateTime& OutNewestTime);

	/**
	 *	Determine the given packages dependent hash
	 *
	 *	@param	InPackageName		The package to process
	 *	@param	OutDependentHash	The hash of this package and dependent packages
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool DeterminePackageDependentHash(const TCHAR* InPackageName, FMD5Hash& OutDependentHash);

	/**
	 *	Determine the given packages file full hash
	 *
	 *	@param	InPackageName		The package to process
	 *	@param	OutNewestTime		The dependent time stamp for the package.
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool DetermineFullPackageHash(const TCHAR* InPackageName, FMD5Hash& OutFullPackageHash);


	virtual bool RecursiveGetDependentPackageHashes(const TCHAR* InPackageName, TMap<FString, FMD5Hash>& Dependencies);

	/**
	 *	Determine dependent timestamps for the given list of files
	 *
	 *	@param	InPackageList		The list of packages to process
	 */
	virtual void DetermineDependentTimeStamps(const TArray<FString>& InPackageList);

	/**
	 *	Determine all found content files dependent timestamps
	 */
	virtual void DetermineAllDependentTimeStamps();

	virtual void AsyncDetermineAllDependentPackageInfo(const TArray<FString>& PackageNames, int32 NumThreads);

	/**
	 *	Get a list of all dependent package information
	 */
	virtual void GetAllPackageDependentInfo(TMap<FString, FPackageDependencyTrackingInfo*>& OutPkgDependencyInfo);


	/**
	* Get the sorted list of generated package dependencies
	*
	* @param PackageName name of the package to get dependencies for
	* @param Dependencies list of dependencies
	* @return true if succeeded and package exists false if package doesn't exist
	*/
	virtual bool GetPackageDependencies( const TCHAR* PackageName, TArray<FString>& OutDependencies );
protected:
	static class FPackageDependencyInfo* PackageDependencyInfo;
};
