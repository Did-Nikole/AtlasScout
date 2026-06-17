#!/usr/bin/env bash
# ==============================================================================
# Midd - Automated Release Script
# ==============================================================================
# This script automates building the Debian and RPM packages with the version
# read from the version file, tagging the commit, pushing the tag to GitHub,
# and creating a GitHub Release with both packages attached.
#
# Usage: ./release.sh
# ==============================================================================

set -euo pipefail

# 1. Read version from version file
if [ ! -f "version" ]; then
    echo "Error: version file not found."
    exit 1
fi

VERSION=$(awk -F: '/version/ {print $2}' version)
if [ -z "$VERSION" ]; then
    echo "Error: Could not read version from version file."
    exit 1
fi

# Strip leading 'v' if present for package naming
CLEAN_VERSION="${VERSION#v}"
TAG_NAME="v${CLEAN_VERSION}"

echo "============================================="
echo " Starting Release Process for Midd ${TAG_NAME}"
echo "============================================="

# 2. Verify that git working directory is clean
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Working directory has uncommitted changes. Please commit or stash them first."
    exit 1
fi

# 3. Build the deb and rpm packages
echo "Building packages for version ${CLEAN_VERSION}..."
make deb PACKAGE_VERSION="${CLEAN_VERSION}"
make rpm PACKAGE_VERSION="${CLEAN_VERSION}"

# Check that the package files actually exist
ARCH=$(dpkg --print-architecture 2>/dev/null || echo "amd64")
DEB_FILE="dist/midd_${CLEAN_VERSION}_${ARCH}.deb"
if [ ! -f "${DEB_FILE}" ]; then
    echo "Error: Debian package file ${DEB_FILE} was not created."
    exit 1
fi

RPM_ARCH=$(rpm --eval '%_arch' 2>/dev/null || uname -m)
RPM_FILE="dist/midd-${CLEAN_VERSION}-1.${RPM_ARCH}.rpm"
if [ ! -f "${RPM_FILE}" ]; then
    echo "Error: RPM package file ${RPM_FILE} was not created."
    exit 1
fi

# 4. Create and push the Git tag
echo "Creating and pushing Git tag ${TAG_NAME}..."
git tag -a "${TAG_NAME}" -m "Release version ${CLEAN_VERSION}"
git push main "${TAG_NAME}"

# 5. Create the GitHub release and upload both packages
echo "Creating GitHub Release and uploading packages..."
gh release create "${TAG_NAME}" "${DEB_FILE}" "${RPM_FILE}" --title "${TAG_NAME}" --notes "Release version ${CLEAN_VERSION}"

echo "============================================="
echo " Release ${TAG_NAME} successfully published!"
echo "============================================="
