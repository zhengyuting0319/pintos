cd ~/PintosProj3/pintos-anon/src/vm
make clean
make
cd build

# pintos -v --gdb --qemu  --filesys-size=2 -p tests/vm/pt-grow-stack -a pt-grow-stack --swap-size=4 -- -q  -f run pt-grow-stack
pintos -v -k -T 1 --qemu  --filesys-size=2 -p tests/vm/pt-grow-stack -a pt-grow-stack --swap-size=4 -- -q  -f run pt-grow-stack

# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/args-single -a args-single --swap-size=4 -- -q  -f run 'args-single onearg'
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/vm/pt-grow-bad -a pt-grow-bad --swap-size=4 -- -q  -f run pt-grow-bad
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/vm/mmap-zero -a mmap-zero --swap-size=4 -- -q  -f run mmap-zero
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/vm/mmap-null -a mmap-null -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run mmap-null
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/vm/mmap-misalign -a mmap-misalign -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run mmap-misalign
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/args-none -a args-none -- -q  -f run args-none
# pintos -v --gdb  --filesys-size=2 -p tests/userprog/args-single -a args-single -- -q  -f run 'args-single onearg'
# pintos -v --gdb --qemu  --filesys-size=2 -p tests/userprog/args-single -a args-single -- -q  -f run 'args-single onearg'
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/exec-once -a exec-once -p tests/userprog/child-simple -a child-simple -- -q  -f run exec-once 
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/read-normal -a read-normal -p ../../tests/userprog/sample.txt -a sample.txt -- -q  -f run read-normal
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/bad-read -a bad-read -- -q  -f run bad-read 
# pintos -v --gdb --qemu  --filesys-size=2 -p tests/userprog/bad-read -a bad-read -- -q  -f run bad-read 
# pintos -v -k -T 50 --qemu  --filesys-size=2 -p tests/userprog/exec-once -a exec-once -p tests/userprog/child-simple -a child-simple -- -q  -f run exec-once 
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/rox-simple -a rox-simple -- -q  -f run rox-simple 
# pintos -v -k -T 360 --qemu  --filesys-size=2 -p tests/userprog/no-vm/multi-oom -a multi-oom -- -q  -f run multi-oom
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/rox-simple -a rox-simple -- -q  -f run rox-simple
# pintos -v -k -T 5 --qemu  --filesys-size=2 -p tests/userprog/rox-child -a rox-child -p tests/userprog/child-rox -a child-rox -- -q  -f run rox-child 
# pintos -v --gdb --qemu  --filesys-size=2 -p tests/userprog/no-vm/multi-oom -a multi-oom -- -q  -f run multi-oom
