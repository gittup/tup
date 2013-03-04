tup.creategitignore()

pages = tup.var{}
examples = tup.var{}
flags = tup.var{}

if tup.getconfig('TUP_WWW') == 'y'
then
	pages:insert 'index.html'
	pages:insert 'getting_started.html'
	pages:insert 'examples.html'
	pages:insert 'manual.html'
	pages:insert 'tips_and_tricks.html'
	pages:insert 'make_vs_tup.html'
	pages:insert 'tup_vs_mordor.html'
	pages:insert 'license.html'
	pages:insert 'support.html'

	examples:insert 'ex_a_first_tupfile.html'
	examples:insert 'ex_dependencies.html'
	examples:insert 'ex_generated_header.html'
	examples:insert 'ex_multiple_directories.html'

	-- Use the 'examples' sub-menu for the examples page.
	flags_specific['examples']:insert '-x'

	if tup.getconfig('TUP_WWW_ANALYTICS') == 'y'
	then
		flags:insert '-a'
	end

	tup.rule(nil, '^ GEN %o^ ./gen_menu.sh $(pages) > %o', 'menu.inc')
	tup.rule(nil, '^ GEN %o^ ./gen_ex_header.sh $(examples) > %o', 'examples.inc')
	tup.rule('examples.inc', '^ GEN %o^ ./gen_menu.sh -x $(pages) > %o', 'menu-examples.inc')
	tup.rule(nil, '^ GEN %o^ ./gen_examples.sh $(examples) > %o', 'examples.html')
	tup.rule('../../tup.1', '^ man2html %o^ man2html %f > %o', 'manual.html')
	for index, page in ipairs(pages)
	do
		tup.rule(
			{page, 'menu.inc', 'menu-examples.inc'}, 
			'^ GEN %o^ ./gen_page.sh $(flags) ' .. 
				' ' .. flags[string.gsub(page, '%..*', '')] .. 
				' ' .. page .. ' > %o', 
			page .. '.gen')
	end
	for index, example in ipairs(examples)
	do
		local outputs = {example .. '.gen'}
		definerule {
			inputs = {example, 'menu-examples.inc'},
			outputs = outputs,
			command = '^ GEN ' .. outputs[1] .. '^' ..
				'./gen_page.sh $(flags) ' ..
				' -x ' .. example .. ' > ' .. outputs[1]
		}
	end
end
