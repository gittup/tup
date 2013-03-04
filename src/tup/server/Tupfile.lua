for index, file in ipairs({'fuse_server.c', 'fuse_fs.c', 'master_fork.c'}) do bang_cc(file) end
bang_mingwcc('windepfile.c')
