rm -rf testfolder/
LD_LIBRARY_PATH=$.:$LD_LIBRARY_PATH gdb --args ./hpa2oar --hpa "hpatest1/untitled.hpa" --oar testfolder
