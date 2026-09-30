vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SourcePath
    REPO NVIDIA/nvapi
    REF 70d337db9186e968eab622f7e786de7e437faf3d
    SHA512 20c5e46510f9b5a8ca387d97af9f78424e4a549e78632e356808f3c203b1ac1ecd3ce7f113abf7aa2877284e7c678be739ac56be246158e22519296eeda1b655
)

file(GLOB Headers "${SourcePath}/*.h")
file(INSTALL ${Headers} DESTINATION "${CURRENT_PACKAGES_DIR}/include/nvapi")
file(INSTALL "${SourcePath}/amd64/nvapi64.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
if(NOT VCPKG_BUILD_TYPE)
    file(INSTALL "${SourcePath}/amd64/nvapi64.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
endif()

vcpkg_install_copyright(FILE_LIST "${SourcePath}/License.txt")
