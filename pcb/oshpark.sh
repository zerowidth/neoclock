#!/usr/bin/env bash
# Package up the gerber and drill files for OSHPark

set -e
base=neoclock

mkdir -p build
cp gerber/$base-F_Cu.gbr build/$base.GTL
cp gerber/$base-B_Cu.gbr build/$base.GBL
cp gerber/$base-F_Mask.gbr build/$base.GTS
cp gerber/$base-B_Mask.gbr build/$base.GBS
cp gerber/$base-F_SilkS.gbr build/$base.GTO
cp gerber/$base-B_SilkS.gbr build/$base.GBO
cp gerber/$base-Edge_Cuts.gbr build/$base.GKO
cp gerber/$base.drl build/$base.drl

pushd build
zip $base.zip $base*
popd
mv build/$base.zip .
rm -rfv build
