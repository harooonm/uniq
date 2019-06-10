# uniq

	Like GNU uniq, but does not require the input to be already **sorted** and does not  have **memory leaks**
    performs **slightly** i.e ~30% faster than the GNU version and is wayyyy smaller than the GNU version.

# TODO
 	1. Lots of testing (mmm maybe)
 	2. Performance bench marks (In Progress)

# Build
 	Just run ./build.sh [debug|small|fast]

# Running
	set the LD_LIBRARY_PATH to include libfolder

# Note
	Please feel free to send bugs, memory leak reports at **maqsood3525@live.com**
# Perf
	To see real differnece between performance(s) , run
	NOTE* do this at your own risk as echo 3 will drop inode,
	dentry and page caches , if you just want to drop inode and
	dentry cache echo 2 for more information see man proc(5)
	sync -f; echo 3 > /proc/sys/vm/drop_caches
	and then run uniq and this uniq on a file, note that GNU uniq will require
	sort to be run to produce the correct output.

