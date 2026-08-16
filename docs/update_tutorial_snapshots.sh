#!/usr/bin/env bash
# Regenerates docs/tutorials/ -- a plain-markdown, GitHub-browsable copy of
# the same tutorial pages the 'docs' CMake target builds for Doxygen (see
# README.md's "Generating Documentation" section and
# .github/workflows/docs.yml, which publishes the full Doxygen site to
# GitHub Pages on every push to master instead).
#
# That Pages site is always fresh; this script's output is NOT -- it's a
# committed snapshot for people browsing the repo on github.com directly
# (GitHub renders README-style markdown/images in the file tree; it doesn't
# render the full Doxygen HTML site inline). Re-run and commit the result
# whenever a demo's output changes enough to be worth refreshing.
#
# One cosmetic wart carried over from the Doxygen pipeline: each page's
# top heading has a trailing `{#some_label}` Doxygen anchor (e.g.
# "# Convolution Tutorial {#convolution_tutorial}") that GitHub's markdown
# renderer shows literally instead of interpreting -- harmless, just not
# pretty.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build"
out_dir="${repo_root}/docs/tutorials"

if [ ! -f "${build_dir}/CMakeCache.txt" ]; then
    cmake -B "${build_dir}" -DNDL_BUILD_TESTS=OFF -DNDL_BUILD_DEMOS=ON
fi
cmake --build "${build_dir}" --target multiview convolution morphology histogram distance_transform summed_area_table motion ct_reconstruction ct_reconstruction_3d nd_viewer -j"$(nproc)"

rm -rf "${out_dir}"
mkdir -p "${out_dir}" "${build_dir}/docs/captured"

declare -A titles=(
    [multiview_tutorial]="Multiview Tutorial"
    [convolution_tutorial]="Convolution Tutorial"
    [morphology_tutorial]="Morphology Tutorial"
    [histogram_tutorial]="Histogram Tutorial"
    [distance_transform_tutorial]="Distance Transform Tutorial"
    [summed_area_table_tutorial]="Summed-Area Table Tutorial"
    [motion_tutorial]="Motion (Optical Flow / SIFT) Tutorial"
    [ct_reconstruction_tutorial]="CT Reconstruction Tutorial"
    [ct_reconstruction_3d_tutorial]="3D Cone-Beam CT Reconstruction Tutorial"
    [nd_viewer_tutorial]="N-D Viewer Tutorial"
)

for target in multiview convolution morphology histogram distance_transform summed_area_table motion ct_reconstruction ct_reconstruction_3d nd_viewer; do
    label="${target}_tutorial"
    bin_dir="${build_dir}/demo/${target}"
    capture_file="${build_dir}/docs/captured/${target}.txt"
    "${bin_dir}/${target}" > "${capture_file}"
    python3 "${repo_root}/docs/generate_tutorial.py" \
        "${capture_file}" "${label}" "${titles[${label}]}" \
        "${bin_dir}/output" "${out_dir}"
done

{
    echo "# Tutorials"
    echo
    echo "Generated snapshots of each demo's own walkthrough -- see \`docs/update_tutorial_snapshots.sh\`."
    echo "These regenerate automatically for [the hosted Doxygen site](https://nathanpackard.github.io/ndl/) on every push to master; this copy is refreshed manually for browsing directly on GitHub."
    echo
    echo "- [Multiview Tutorial](multiview_tutorial.md)"
    echo "- [Convolution Tutorial](convolution_tutorial.md)"
    echo "- [Morphology Tutorial](morphology_tutorial.md)"
    echo "- [Histogram Tutorial](histogram_tutorial.md)"
    echo "- [Distance Transform Tutorial](distance_transform_tutorial.md)"
    echo "- [Summed-Area Table Tutorial](summed_area_table_tutorial.md)"
    echo "- [Motion (Optical Flow / SIFT) Tutorial](motion_tutorial.md)"
    echo "- [CT Reconstruction Tutorial](ct_reconstruction_tutorial.md)"
    echo "- [3D Cone-Beam CT Reconstruction Tutorial](ct_reconstruction_3d_tutorial.md)"
    echo "- [N-D Viewer Tutorial](nd_viewer_tutorial.md)"
} > "${out_dir}/README.md"

echo "wrote ${out_dir}"
