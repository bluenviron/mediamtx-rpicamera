#!/bin/sh -e

MACHINE=$(gcc -dumpmachine)

if [ "$MACHINE" = "aarch64-linux-gnu" ]; then
    OUT_DIR="mtxrpicam_64"
else
    OUT_DIR="mtxrpicam_32"
fi

mkdir -p $OUT_DIR
cp -r ${DESTDIR}/${MESON_INSTALL_PREFIX}/share/libcamera/ipa $OUT_DIR/ipa_conf
cp -r ${DESTDIR}/${MESON_INSTALL_PREFIX}/lib/libcamera $OUT_DIR/ipa_module
cp ${DESTDIR}/${MESON_INSTALL_PREFIX}/lib/libcamera-base.so.9.9 $OUT_DIR/
cp ${DESTDIR}/${MESON_INSTALL_PREFIX}/lib/libcamera.so.9.9 $OUT_DIR/
cp ${DESTDIR}/${MESON_INSTALL_PREFIX}/bin/mtxrpicam $OUT_DIR/
