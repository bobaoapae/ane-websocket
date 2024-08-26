#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Check for optional arguments
MODE="all"
if [[ $1 == "compile" ]]; then
    MODE="compile"
elif [[ $1 == "universal" ]]; then
    MODE="universal"
fi

# Manually set the versions and URLs
OPENSSL_VERSION="3.3.1"  # Set your desired OpenSSL version here
OPENSSL_URL="https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
BOOST_VERSION="1.82.0"    # Set your desired Boost version here
BOOST_DIR="boost_${BOOST_VERSION//./_}"

# Directories for build and install
BUILD_DIR="$HOME/universal-build"
INSTALL_DIR="$HOME/universal-install"
OPENSSL_BUILD_DIR="$BUILD_DIR/openssl"
BOOST_BUILD_DIR="$BUILD_DIR/boost"
OPENSSL_INSTALL_DIR="$INSTALL_DIR/openssl"
BOOST_INSTALL_DIR="$INSTALL_DIR/boost"

# Create directories
mkdir -p "$OPENSSL_BUILD_DIR" "$BOOST_BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$OPENSSL_INSTALL_DIR/x86_64" "$OPENSSL_INSTALL_DIR/arm64"
mkdir -p "$BOOST_INSTALL_DIR/x86_64" "$BOOST_INSTALL_DIR/arm64"

echo "Using OpenSSL version: $OPENSSL_VERSION from $OPENSSL_URL"
echo "Using Boost version: $BOOST_VERSION"

# Function to build OpenSSL
build_openssl() {
    ARCH=$1
    PREFIX=$2
    cd "$OPENSSL_BUILD_DIR"
    
    # Download and extract OpenSSL from the provided URL
    curl -L "$OPENSSL_URL" -o "openssl-${OPENSSL_VERSION}.tar.gz"
    tar -xzf "openssl-${OPENSSL_VERSION}.tar.gz"
    cd "openssl-${OPENSSL_VERSION}"

    if [ "$ARCH" == "x86_64" ]; then
        ./Configure darwin64-x86_64-cc --prefix="$PREFIX"
    else
        ./Configure darwin64-arm64-cc --prefix="$PREFIX"
    fi

    make clean
    make -j$(sysctl -n hw.logicalcpu)
    make install_sw
}

# Function to build Boost including libraries needed for Boost Beast
build_boost() {
    ARCH=$1
    PREFIX=$2
    cd "$BOOST_BUILD_DIR"
    
    # Download and extract the specific Boost version
    curl -L "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/${BOOST_DIR}.tar.gz" -o "${BOOST_DIR}.tar.gz"
    tar -xzf "${BOOST_DIR}.tar.gz"
    cd "$BOOST_DIR"

    ./bootstrap.sh --with-libraries=system,thread,filesystem --prefix="$PREFIX"

    if [ "$ARCH" == "x86_64" ]; then
        ./b2 clean
        ./b2 install toolset=clang cxxflags="-arch x86_64" linkflags="-arch x86_64" --prefix="$PREFIX" --build-dir="$BOOST_BUILD_DIR/x86_64-build"
    else
        ./b2 clean
        ./b2 install toolset=clang cxxflags="-arch arm64" linkflags="-arch arm64" --prefix="$PREFIX" --build-dir="$BOOST_BUILD_DIR/arm64-build"
    fi
}

# Clean any existing build artifacts
if [[ "$MODE" != "universal" ]]; then
    echo "Cleaning existing build artifacts..."
    rm -rf "$BUILD_DIR" "$INSTALL_DIR"
    mkdir -p "$OPENSSL_BUILD_DIR" "$BOOST_BUILD_DIR" "$INSTALL_DIR"
    mkdir -p "$OPENSSL_INSTALL_DIR/x86_64" "$OPENSSL_INSTALL_DIR/arm64"
    mkdir -p "$BOOST_INSTALL_DIR/x86_64" "$BOOST_INSTALL_DIR/arm64"
fi

# Build OpenSSL and Boost for specified architectures
if [[ "$MODE" == "all" || "$MODE" == "compile" ]]; then
    echo "Building OpenSSL for x86_64..."
    build_openssl "x86_64" "$OPENSSL_INSTALL_DIR/x86_64"

    echo "Building OpenSSL for arm64..."
    build_openssl "arm64" "$OPENSSL_INSTALL_DIR/arm64"

    echo "Building Boost for x86_64..."
    build_boost "x86_64" "$BOOST_INSTALL_DIR/x86_64"

    echo "Building Boost for arm64..."
    build_boost "arm64" "$BOOST_INSTALL_DIR/arm64"
fi

# Combine libraries into universal binaries
if [[ "$MODE" == "all" || "$MODE" == "universal" ]]; then
    echo "Creating universal binaries for OpenSSL libraries..."
    for lib in "$OPENSSL_INSTALL_DIR/x86_64/lib/"*.a "$OPENSSL_INSTALL_DIR/x86_64/lib/"*.dylib; do
        libname=$(basename "$lib")
        if [ -f "$OPENSSL_INSTALL_DIR/arm64/lib/$libname" ]; then
            lipo -create "$OPENSSL_INSTALL_DIR/x86_64/lib/$libname" "$OPENSSL_INSTALL_DIR/arm64/lib/$libname" -output "$INSTALL_DIR/$libname"
        fi
    done

    echo "Creating universal binaries for Boost libraries..."
    for lib in "$BOOST_INSTALL_DIR/x86_64/lib"/*.a "$BOOST_INSTALL_DIR/x86_64/lib"/*.dylib; do
        libname=$(basename "$lib")
        if [ -f "$BOOST_INSTALL_DIR/arm64/lib/$libname" ]; then
            lipo -create "$BOOST_INSTALL_DIR/x86_64/lib/$libname" "$BOOST_INSTALL_DIR/arm64/lib/$libname" -output "$INSTALL_DIR/$libname"
        fi
    done
fi

# Clean up
if [[ "$MODE" != "universal" ]]; then
    echo "Cleaning up build directories..."
    rm -rf "$BUILD_DIR"
fi

echo "Build completed. Libraries are in $INSTALL_DIR"