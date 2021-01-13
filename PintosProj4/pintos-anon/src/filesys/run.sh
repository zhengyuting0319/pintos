cd ~/PintosProj4/pintos-anon/src/filesys
make clean
make
cd build

pintos-mkdisk tmp.dsk --filesys-size=2
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-rm-tree -a dir-rm-tree -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-rm-tree < /dev/null 2> tests/filesys/extended/dir-rm-tree.errors > tests/filesys/extended/dir-rm-tree.output
# pintos -v -k -T 5  --qemu --disk=tmp.dsk -g fs.tar -a tests/filesys/extended/dir-rm-tree.tar --swap-size=4 -- -q  run 'tar fs.tar /' < /dev/null 2> tests/filesys/extended/dir-rm-tree-persistence.errors > tests/filesys/extended/dir-rm-tree-persistence.output
# perl -I../.. ../../tests/filesys/extended/dir-rm-tree.ck tests/filesys/extended/dir-rm-tree tests/filesys/extended/dir-rm-tree.result
# perl -I../.. ../../tests/filesys/extended/dir-rm-tree-persistence.ck tests/filesys/extended/dir-rm-tree-persistence tests/filesys/extended/dir-rm-tree-persistence.result

pintos -v -k -T 150 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-vine -a dir-vine -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-vine < /dev/null 2> tests/filesys/extended/dir-vine.errors > tests/filesys/extended/dir-vine.output
pintos -v -k -T 5  --qemu --disk=tmp.dsk -g fs.tar -a tests/filesys/extended/dir-vine.tar --swap-size=4 -- -q  run 'tar fs.tar /' < /dev/null 2> tests/filesys/extended/dir-vine-persistence.errors > tests/filesys/extended/dir-vine-persistence.output
rm -f tmp.dsk
perl -I../.. ../../tests/filesys/extended/dir-vine.ck tests/filesys/extended/dir-vine tests/filesys/extended/dir-vine.result
perl -I../.. ../../tests/filesys/extended/dir-vine-persistence.ck tests/filesys/extended/dir-vine-persistence tests/filesys/extended/dir-vine-persistence.result


# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-empty-name -a dir-empty-name -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-empty-name < /dev/null 2> tests/filesys/extended/dir-empty-name.errors > tests/filesys/extended/dir-empty-name.output
# pintos -v -k -T 5  --qemu --disk=tmp.dsk -g fs.tar -a tests/filesys/extended/dir-empty-name.tar --swap-size=4 -- -q  run 'tar fs.tar /' < /dev/null 2> tests/filesys/extended/dir-empty-name-persistence.errors > tests/filesys/extended/dir-empty-name-persistence.output
# perl -I../.. ../../tests/filesys/extended/dir-empty-name-persistence.ck tests/filesys/extended/dir-empty-name-persistence tests/filesys/extended/dir-empty-name-persistence.result
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-rm-parent -a dir-rm-parent -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-rm-parent
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-rm-cwd -a dir-rm-cwd -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-rm-cwd 
# pintos -v -k -T 15 --qemu  --filesys-size=2 -p tests/userprog/write-normal -a write-normal -p ../../tests/userprog/sample.txt -a sample.txt --swap-size=4 -- -q  -f run write-normal
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-open -a dir-open -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-open
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-empty-name -a dir-empty-name -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-empty-name
# pintos -v -k -T 5  --qemu --disk=tmp.dsk -g fs.tar -a tests/filesys/extended/dir-empty-name.tar --swap-size=4 -- -q  run 'tar fs.tar /' 
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-mk-tree -a dir-mk-tree -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-mk-tree 
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-mkdir -a dir-mkdir -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-mkdir < /dev/null 2> tests/filesys/extended/dir-mkdir.errors 
# pintos -v -k -T 15 --qemu  --disk=tmp.dsk -p tests/filesys/extended/dir-empty-name -a dir-empty-name -p tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run dir-empty-name 