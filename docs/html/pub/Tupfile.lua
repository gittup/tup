if tup.getconfig('TUP_WWW') == 'y'
then
	for index, file in ipairs(tup.glob('../make*.dat'))
		do tup.rule(file, '^ PLOT %f^ ../../../test/make_v_tup/plot %f > %o', '%B.png') end
	for index, file in ipairs(tup.glob('../tup_vs_eye*.dat'))
		do tup.rule(file, '^ PLOT %f^ ../../../test/make_v_tup/plot %f Eye > %o', '%B.png') end
	for index, file in ipairs(tup.glob('../*.dot')) 
		do bang_dot(file, string.gsub(string.gsub(file, '../', ''), '\\.dot', '') .. '.png') end
	for index, file in ipairs(tup.glob('../*.gen'))
		do tup.rule(file, '^ CP %o^ cp %f %o', '%B') end
end

