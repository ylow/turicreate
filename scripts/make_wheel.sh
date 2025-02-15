#!/bin/bash

set -e

PYTHON_SCRIPTS=deps/env/bin
if [[ $OSTYPE == msys ]]; then
  PYTHON_SCRIPTS=deps/conda/bin/Scripts
fi

SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
WORKSPACE=${SCRIPT_DIR}/..
build_type="release"

# The build image version that will be used for building
TC_BUILD_IMAGE_CENTOS_6=$(bash $WORKSPACE/scripts/get_docker_image.sh --centos=6)

unknown_option() {
  echo "Unknown option $1. Exiting."
  exit 1
}

print_help() {
  echo "Builds the release branch and produce a wheel to the targets directory "
  echo
  echo "Usage: ./make_wheel.sh --version=[version] [remaining options]"
  echo
  echo "  --build_number=[version] The build number of the wheel, e.g. 123. Or 123.gpu"
  echo
  echo "  --skip_test              Skip unit tests (Python & C++)."
  echo
  echo "  --skip_cpp_test          Skip C++ unit tests. C++ tests are default skipped on Windows."
  echo
  echo "  --skip_build             Skip the build process."
  echo
  echo "  --skip_smoke_test        Skip importing the wheel and running a quick smoke test."
  echo
  echo "  --debug                  Use debug build instead of release."
  echo
  echo "  --docker-python2.7       Use docker to build for Python 2.7 in CentOS 6 with Clang 8."
  echo
  echo "  --docker-python3.5       Use docker to build for Python 3.5 in CentOS 6 with Clang 8."
  echo
  echo "  --docker-python3.6       Use docker to build for Python 3.6 in CentOS 6 with Clang 8."
  echo
  echo "  --docker-python3.7       Use docker to build for Python 3.7 in CentOS 6 with Clang 8."
  echo
  echo "  --docker-python3.8       Use docker to build for Python 3.8 in CentOS 6 with Clang 8."
  echo
  echo "  --docker-python3.9       Use docker to build for Python 3.9 in CentOS 6 with Clang 8."
  echo
  echo "  --num_procs=n            Specify the number of proceses to run in parallel."
  echo
  echo "  --target-dir=[dir]       The directory where the wheel and associated files are put."
  echo
  echo "Produce a local wheel and skip test"
  echo "Example: ./make_wheel.sh --skip_test"
  exit 1
} # end of print help

# command flag options
# Parse command line configure flags ------------------------------------------
while [ $# -gt 0 ]
  do case $1 in
    --build_number=*)       BUILD_NUMBER=${1##--build_number=} ;;
    --target-dir=*)         TARGET_DIR=${1##--target-dir=} ;;
    --num_procs=*)          NUM_PROCS=${1##--num_procs=} ;;
    --skip_test)            SKIP_TEST=1;;
    --skip_cpp_test)        SKIP_CPP_TEST=1;;
    --skip_build)           SKIP_BUILD=1;;
    --skip_smoke_test)      SKIP_SMOKE_TEST=1;;
    --release)              build_type="release";;
    --debug)                build_type="debug";;
    --docker-python2.7)     USE_DOCKER=1;DOCKER_PYTHON=2.7;;
    --docker-python3.5)     USE_DOCKER=1;DOCKER_PYTHON=3.5;;
    --docker-python3.6)     USE_DOCKER=1;DOCKER_PYTHON=3.6;;
    --docker-python3.7)     USE_DOCKER=1;DOCKER_PYTHON=3.7;;
    --docker-python3.8)     USE_DOCKER=1;DOCKER_PYTHON=3.8;;
    --docker-python3.9)     USE_DOCKER=1;DOCKER_PYTHON=3.9;;
    --help)                 print_help ;;
    *) unknown_option $1 ;;
  esac
  shift
done


set -x

if [[ -z "${BUILD_NUMBER}" ]]; then
  BUILD_NUMBER="0"
fi

if [[ -z "${NUM_PROCS}" ]]; then
  NUM_PROCS=4
fi

if [[ -z "${TARGET_DIR}" ]]; then
  TARGET_DIR=${WORKSPACE}/target
fi

# If we are going to run in Docker,
# send this command into Docker and bail out of here when done.
if [[ -n "${USE_DOCKER}" ]]; then
  # create the build and test images
  # (this should ideally be a no-op if the image exists and is current)
  $WORKSPACE/scripts/create_docker_images.sh

  # set up arguments to make_wheel.sh within docker
  # always skip smoke test since it (currently) fails on CentOS 6
  # always skip doc gen since it (currently) fails on CentOS 6
  make_wheel_args="--build_number=$BUILD_NUMBER --num_procs=$NUM_PROCS --skip_test"
  if [[ -n $SKIP_BUILD ]]; then
    make_wheel_args="$make_wheel_args --skip_build"
  fi
  if [[ -n $SKIP_CPP_TEST ]]; then
    make_wheel_args="$make_wheel_args --skip_cpp_test"
  fi
  if [[ -n $SKIP_SMOKE_TEST ]]; then
    make_wheel_args="$make_wheel_args --skip_smoke_test"
  fi
  if [[ "$build_type" == "debug" ]]; then
    make_wheel_args="$make_wheel_args --debug"
  fi

  # Run the make_wheel.sh script inside Docker to build
  # 8 GB seems to be the minimum RAM - at 4, we get out-of-memory
  # linking libunity_shared.so.
  docker run --rm -m=8g \
    --mount type=bind,source=$WORKSPACE,target=/build,consistency=delegated \
    -e "VIRTUALENV=virtualenv --python=python${DOCKER_PYTHON}" \
    ${TC_BUILD_IMAGE_CENTOS_6} \
    /build/scripts/make_wheel.sh \
    $make_wheel_args

  # Delete env to force re-creation of virtualenv if we are running tests next
  # (to prevent reuse of CentOS 6 virtualenv on 14.04/18.04)
  docker run --rm -m=4g \
    --mount type=bind,source=$WORKSPACE,target=/build,consistency=delegated \
    -e "VIRTUALENV=virtualenv --python=python${DOCKER_PYTHON}" \
    ${TC_BUILD_IMAGE_CENTOS_6} \
    rm -rf /build/deps/env

  # Run the tests inside Docker (14.04) if desired
  # CentOS 6 is not capable of passing turicreate unit tests currently
  if [[ -z $SKIP_TEST ]]; then
    # run the tests
    ./scripts/test_wheel.sh --docker-python${DOCKER_PYTHON}
  fi

  # Delete env to force re-creation of virtualenv for next build
  # (to prevent reuse of 14.04/18.04 virtualenv on CentOS 6)
  docker run --rm -m=4g \
    --mount type=bind,source=$WORKSPACE,target=/build,consistency=delegated \
    -e "VIRTUALENV=virtualenv --python=python${DOCKER_PYTHON}" \
    ${TC_BUILD_IMAGE_CENTOS_6} \
    rm -rf /build/deps/env

  exit 0
fi

mkdir -p ${TARGET_DIR}

# Windows specific
archive_file_ext="whl"
if [[ $OSTYPE == msys ]]; then
  # C++ tests are default skipped on windows due to annoying issues around
  # unable to find libstdc++.dll which are not so easily fixable
  SKIP_CPP_TEST=1
fi

push_ld_library_path() {
  OLD_LIBRARY_PATH=$LD_LIBRARY_PATH
  export LD_LIBRARY_PATH=${WORKSPACE}/deps/local/lib:${WORKSPACE}/deps/local/lib64:$LD_LIBRARY_PATH
}

pop_ld_library_path() {
  export LD_LIBRARY_PATH=$OLD_LIBRARY_PATH
}

### Build the source with version number ###
build_source() {
  echo -e "\n\n\n================= Build Release: VERSION ${BUILD_NUMBER} ================\n\n\n"

  # Configure
  cd ${WORKSPACE}

  ./configure

  push_ld_library_path

  # make src/
  cd ${WORKSPACE}/${build_type}/src
  make -j${NUM_PROCS}

  # make test/
  if [[ -z $SKIP_TEST ]]; then
    if [[ -z $SKIP_CPP_TEST ]]; then
      cd ${WORKSPACE}/test
      touch $(find . -name "*.cxx")
      cd ${WORKSPACE}/${build_type}/test
      make -j${NUM_PROCS}
    fi
  fi
  pop_ld_library_path

  echo -e "\n\n================= Done Build Source ================\n\n"
}


# Run all unit test
cpp_test() {
  echo -e "\n\n\n================= Running C++ Unit Tests ================\n\n\n"
  cd ${WORKSPACE}/${build_type}/test
  push_ld_library_path
  ${WORKSPACE}/scripts/run_cpp_tests.py -j${NUM_PROCS}
  pop_ld_library_path
  echo -e "\n\n\n================= Done C++ Unit Tests ================\n\n\n"
}

# Run all unit test
unit_test() {
  echo -e "\n\n\n================= Running Python Unit Tests ================\n\n\n"

  cd ${WORKSPACE}
  scripts/run_python_test.sh ${build_type}
  echo -e "\n\n================= Done Python Unit Tests ================\n\n"
}

mac_patch_rpath() {
  echo -e "\n\n\n================= Patching Mac RPath ================\n\n\n"
  # on mac we need to do a little work to fix up RPaths
  cd ${WORKSPACE}/${build_type}/src/python
  # - look for all files
  # - run 'file' on it
  # - look for binary files (shared libraries, executables)
  # - output is in the form [file]: type, so cut on ":", we just want the file
  flist=`find . -type f -not -path "*/CMakeFiles/*" -not -path "./dist/*" -not -name "*.py"`

  # We are generally going to be installed in
  # a place of the form
  # PREFIX/lib/python2.7/site-packages/[module]
  # libpython2.7 will probably in PREFIX/lib
  # So for instance if I am in
  # PREFIX/lib/python2.7/site-packages/turicreate/unity_server
  # I need to add
  # ../../../ to the rpath (to make it to lib/)
  # But if I am in
  #
  # PREFIX/lib/python2.7/site-packages/turicreate/something/somewhere
  # I need to add
  # ../../../../ to the rpath
  for f in $flist; do
    # Lets explain the regexes
    # - The first regex removes "./"
    # - The second regex replaces the string "[something]/" with "../"
    #   (making sure something does not contain '/' characters)
    # - The 3rd regex removes the last "filename" bit
    #
    # Example:
    # Input: ./turicreate/cython/cy_ipc.so
    # After 1st regex: turicreate/cython/cy_ipc.so
    # After 2nd regex: ../../cy_ipc.so
    # After 3rd regex: ../../
    relative=`echo $f | sed 's:\./::g' | sed 's:[^/]*/:../:g' | sed 's:[^/]*$::'`
    # Now we need ../../ to get to PREFIX/lib
    rpath="@loader_path/../../$relative"
    install_name_tool -add_rpath $rpath $f || true
  done
}


### Package the release folder into a wheel, and strip the binaries ###
function package_wheel() {
  if [[ $OSTYPE == darwin* ]]; then
    mac_patch_rpath
  fi
  echo -e "\n\n\n================= Packaging Wheel  ================\n\n\n"
  cd ${WORKSPACE}/${build_type}/src/python

  # strip binaries
  if [[ ! $OSTYPE == darwin* ]]; then
    cd ${WORKSPACE}/${build_type}/src/python/turicreate
    BINARY_LIST=`find . -type f -exec file {} \; | grep x86 | cut -d: -f 1`
    echo "Stripping binaries: $BINARY_LIST"

    # make newline the separator for items in for loop - default is whitespace
    OLD_IFS=${IFS}
    IFS=$'\n'

    for f in $BINARY_LIST; do
      if [ $OSTYPE == "msys" ] && [ $f == "./pylambda_worker.exe" ]; then
        echo "Skipping pylambda_worker"
      else
        echo "Stripping $f"
        strip -Sx $f;
      fi
    done

    # set IFS back to default
    IFS=${OLD_IFS}
    cd ..
  fi

  # helper function defined within function package_wheel
  function package_wheel_helper {
    local is_minimal=$1
    local version_modifier=$2
    local dist_type="bdist_wheel"

    cd ${WORKSPACE}/${build_type}/src/python

    if [[ "$is_minimal" -eq 1 ]] ; then
      local pkg_patch_ver=$(grep VERSION_STRING setup.py | cut -d " " -f3)
      pkg_patch_ver="${pkg_patch_ver//\"}${version_modifier}"
      if [[ "$(uname -s)" == "Darwin" ]] ; then
        sed -i "" 's/^USE_MINIMAL = False$/USE_MINIMAL = True/g' turicreate/_deps/minimal_package.py
        sed -i '' "s/\".*\"  # {{VERSION_STRING}}/\"${pkg_patch_ver}\"  # {{VERSION_STRING}}/g" turicreate/version_info.py setup.py
      else
        sed -i 's/^USE_MINIMAL = False$/USE_MINIMAL = True/g' turicreate/_deps/minimal_package.py
        sed -i "s/\".*\"  # {{VERSION_STRING}}/\"${pkg_patch_ver}\"  # {{VERSION_STRING}}/g" turicreate/version_info.py setup.py
      fi
    fi

    # This produced a wheel starting with turicreate-${VERSION_NUMBER} under dist/
    if [[ "${is_minimal}" -eq 1 ]]; then
      "${PYTHON_EXECUTABLE}" setup.py -q "${dist_type}" install --minimal
      # restore the scene after packing; making it reentrant
      cp ${WORKSPACE}/src/python/setup.py setup.py
      cp ${WORKSPACE}/src/python/turicreate/version_info.py turicreate/version.py
    else
      # full version
      "${PYTHON_EXECUTABLE}" setup.py -q "${dist_type}"
    fi

    VERSION_NUMBER=`${PYTHON_EXECUTABLE} -c "from turicreate.version_info import version; print(version)"`

    cd "${WORKSPACE}"

    WHEEL_PATH=$(ls "${WORKSPACE}/${build_type}/src/python/dist/turicreate-${VERSION_NUMBER}"*.whl)

    if [[ $OSTYPE == darwin* ]]; then
      # Change the platform tag embedded in the file name
      temp=`echo $WHEEL_PATH | perl -ne 'print m/(^.*-).*$/'`
      temp=${temp/-cpdarwin-/-cp35m-}

      platform_tag="macosx_10_9_universal2"
      #  sdk_version=`xcrun --show-sdk-version`
      #  if [[ $sdk_version =~ ^10\.13 ]]; then
      #      platform_tag="macosx_10_13_intel.macosx_10_12_x86_64"
      #  elif [[ $sdk_version =~ ^10\.12 ]]; then
      #      platform_tag="macosx_10_12_intel.macosx_10_12_x86_64"
      #  fi

      NEW_WHEEL_PATH=${temp}${platform_tag}".whl"
      mv ${WHEEL_PATH} ${NEW_WHEEL_PATH}
      WHEEL_PATH=${NEW_WHEEL_PATH}
    else
      # Don't pick up -manylinux1 wheels, since those may have been created by a later step from a previous build.
      # Ignore those for now by selecting only -linux wheels.
      WHEEL_PATH=`ls ${WORKSPACE}/${build_type}/src/python/dist/turicreate-${VERSION_NUMBER}*-linux*.whl`
    fi

    # Set Python Language Version Number
    NEW_WHEEL_PATH=${WHEEL_PATH}
    if [[ "$python35" == "1" ]]; then
        NEW_WHEEL_PATH=${WHEEL_PATH/-py3-/-cp35-}
    elif [[ "$python36" == "1" ]]; then
        NEW_WHEEL_PATH=${WHEEL_PATH/-py3-/-cp36-}
    else
        NEW_WHEEL_PATH=${WHEEL_PATH/-py2-/-cp27-}
    fi

    NEW_WHEEL_PATH=${NEW_WHEEL_PATH/linux/manylinux1}
    if [[ ! "${WHEEL_PATH}" == "${NEW_WHEEL_PATH}" ]]; then
        mv "${WHEEL_PATH}" "${NEW_WHEEL_PATH}"
        WHEEL_PATH=${NEW_WHEEL_PATH}
    fi

    if [[ -z $SKIP_SMOKE_TEST ]]; then
      # Install the wheel and do a smoke test
      unset PYTHONPATH

      "$PIP_EXECUTABLE" uninstall -y turicreate
      "$PIP_EXECUTABLE" install "${WHEEL_PATH}"
      $PYTHON_EXECUTABLE -c "import turicreate; turicreate.SArray(range(100)).apply(lambda x: x)"
    fi

    # Done copy to the target directory
    mv "$WHEEL_PATH" "${TARGET_DIR}"
  }

  # Run the setup
  package_wheel_helper 0 ""

  echo -e "\n\n================= Done Packaging Wheel  ================\n\n"
}

set_build_number() {
  # set the build number
  cd "${WORKSPACE}/${build_type}/src/python/"
  sed -i -e "s/\".*\"  # {{BUILD_NUMBER}}/\"${BUILD_NUMBER}\"  # {{BUILD_NUMBER}}/g" turicreate/version_info.py
}

set_git_SHA() {
  # set the git SHA1 revision
  cd ${WORKSPACE}
  GIT_SHA=$(git rev-parse HEAD)
  if [ $? -ne 0 ]; then
    GIT_SHA = "NA"
  fi

  cd "${WORKSPACE}/${build_type}/src/python/"
  sed -i -e "s/\".*\"  # {{GIT_SHA}}/\"${GIT_SHA}\"  # {{GIT_SHA}}/g" turicreate/version_info.py
}

# Here is the main function()
if [[ -z $SKIP_BUILD ]]; then
  build_source
fi

if [[ -z $SKIP_TEST ]]; then
  if [[ -z $SKIP_CPP_TEST ]]; then
    cpp_test
  fi
  unit_test
fi

set_build_number
set_git_SHA

source ${WORKSPACE}/scripts/python_env.sh $build_type

package_wheel
