tup.dorulesfile()
for index, file in ipairs(tup.glob('*.c'))
do
	bang_cc(file)
	bang_mingwcc(file)
end
