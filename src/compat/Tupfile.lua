-- Changes here need to be reflected in the build.sh file
srcs = tup.var {}
mingwsrcs = tup.var {}

if tup.getconfig('TUP_PLATFORM') == 'linux'
then
	srcs:insert 'utimensat_linux.c'
	srcs:insert 'dummy.c'
end

if tup.getconfig('TUP_PLATFORM') == 'freebsd'
then
	srcs:insert 'utimensat_linux.c'
	srcs:insert 'dummy.c'
	srcs:insert 'clearenv.c'
end

if tup.getconfig('TUP_PLATFORM') == 'macosx'
then
	srcs:insert 'clearenv.c'
	srcs:insert 'dir_mutex.c'

	srcs:insert 'faccessat.c'
	srcs:insert 'fchmodat.c'
	srcs:insert 'fchownat.c'
	srcs:insert 'fdopendir.c'
	srcs:insert 'fstatat.c'
	srcs:insert 'mkdirat.c'
	srcs:insert 'openat.c'
	srcs:insert 'readlinkat.c'
	srcs:insert 'renameat.c'
	srcs:insert 'symlinkat.c'
	srcs:insert 'unlinkat.c'
	srcs:insert 'utimensat.c'
end

if tup.getconfig('TUP_PLATFORM') == 'solaris'
then
	srcs:insert 'dir_mutex.c'
	srcs:insert 'mkdirat.c'
	srcs:insert 'readlinkat.c'
end
for index, file in ipairs(srcs) do bang_cc(file) end

mingwsrcs:insert 'dir_mutex.c'
mingwsrcs:insert 'fstatat.c'
mingwsrcs:insert 'mkdirat.c'
mingwsrcs:insert 'openat.c'
mingwsrcs:insert 'renameat.c'
mingwsrcs:insert 'unlinkat.c'
for index, file in ipairs(mingwsrcs) do bang_mingwcc(file) end
