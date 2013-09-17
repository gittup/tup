-- vim: set ts=8 sw=8 sts=8 noet tw=78:
--
-- tup - A file-based build system
--
-- Copyright (C) 2013  Rendaw <rendaw@zarbosoft.com>
-- Copyright (C) 2013  Mike Shal <marfey@gmail.com>
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License version 2 as
-- published by the Free Software Foundation.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License along
-- with this program; if not, write to the Free Software Foundation, Inc.,
-- 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
--

local realtostring = tostring
function tostring(t)
	if type(t) == 'table' and not getmetatable(t) then
		return table.concat(t, ' ')
	end
	return realtostring(t)
end

local realioopen = io.open
function io.open(filename, mode)
	tup.chdir(filename, mode)
	local f = realioopen(filename, mode)
	tup.unchdir()
	return f
end

local conditionalglob = function(input)
	if input:match('%*') or input:match('%?') or input:match('%[.*%]') then
		return tup.glob(input)
	end
	return {input}
end

tup.file = function(filename)
	-- Returns filename sans preceeding dir/'s
	return (string.gsub(filename, '[^/\\]*[/\\]', ''))
end

tup.base = function(filename)
	-- Returns filename sans preceeding dir/'s and sans the final . and following characters
	return (string.gsub(tup.file(filename), '%.%w*$', ''))
end

tup.ext = function(filename)
	-- Returns the part after the final . in filename
	match = string.match(filename, '%.(%w*)$')
	return match and match or ''
end

tup.frule = function(arguments)
	-- Takes inputs, outputs, and commands as in tup.definerule,
	-- additionally accepts, input and output which may be either tables or strings
	-- Replaces $(), @(), and %d in inputs with proper values
	-- Replaces $(), @(), %d, %f, %b, %B in outputs with proper values
	-- Replaces $(), @(), %d, %f, %b, %B, %o in command with proper values
	-- Returns the expanded outputs

	function evalGlobals(text)
		-- Replace $(VAR) with value of global variable VAR
		return text:gsub('%$%(([^%)]*)%)',
			function(var)
				if _G[var] then return tostring(_G[var]) end
				return ''
			end)
	end

	function evalConfig(text)
		-- Replace @(VAR) with value of config variable VAR
		return text:gsub('@%(([^%)]*)%)',
			function(configvar)
				return tup.getconfig(configvar)
			end)
	end

	local inputs
	if arguments.inputs then
		inputs = arguments.inputs
	elseif arguments.input and type(arguments.input) ~= 'table' then
		inputs = { arguments.input }
	elseif arguments.input then
		inputs = arguments.input
	end

	if inputs then
		local newinputs = {}
		local duplicates = {}
		for index, input in ipairs(inputs) do
			-- Explicitly convert to string for node variables.
			local newinput = tostring(input)
			newinput = evalGlobals(newinput)
			newinput = evalConfig(newinput)
			table.insert(newinputs, newinput)
		end
		inputs = newinputs

		newinputs = {}
		for index, input in ipairs(inputs) do
			glob = conditionalglob(input)
			for globindex, globvalue in ipairs(glob) do
				if(not duplicates[globvalue]) then
					duplicates[globvalue] = 1
					table.insert(newinputs, globvalue)
				end
			end
		end
		inputs = newinputs
	end

	local outputs
	if arguments.outputs then
		outputs = arguments.outputs
	elseif arguments.output and type(arguments.output) ~= 'table' then
		if arguments.output == '%f' then
			outputs = inputs
		else
			outputs = {arguments.output}
		end
	elseif arguments.output then
		outputs = arguments.output
	end

	if outputs then
		local newoutputs = {}
		for index, output in ipairs(outputs) do
			local newoutput = output

			newoutput = evalGlobals(newoutput)
			newoutput = evalConfig(newoutput)

			if not inputs or not inputs[1] then
				if output:match('%%b') or output:match('%%B') or output:match('%%e') then
					error 'tup.frule can only use output formatters %b, %B, or %e with exactly one input.'
				end
			else
				newoutput = newoutput
					:gsub('%%b', tup.file(inputs[1]))
					:gsub('%%B', tup.base(inputs[1]))
					:gsub('%%e', tup.ext(inputs[1]))
			end

			newoutput = newoutput:gsub('%%d', tup.getdirectory())

			table.insert(newoutputs, newoutput)
		end
		outputs = newoutputs

		newoutputs = {}
		for index, output in ipairs(outputs) do
			glob = conditionalglob(output)
			for globindex, globvalue in ipairs(glob) do
				table.insert(newoutputs, globvalue)
			end
		end
		outputs = newoutputs
	end

	local command
	if arguments.command then
		command = arguments.command

		local outputreplacement = (outputs and table.concat(outputs, ' '):gsub('%%', '%%%%') or '')
		command = command:gsub('%%o', outputreplacement)

		local inputreplacement = inputs and table.concat(inputs, ' '):gsub('%%', '%%%%') or ''
		command = command:gsub('%%f', inputreplacement)

		if not inputs or not inputs[1] then
			if command:match('%%b') or command:match('%%B') or command:match('%%e') then
				error 'tup.frule can only use command formatters %b, %B, or %e with exactly one input.'
			end
		else
			command = command
				:gsub('%%b', tup.file(inputs[1]))
				:gsub('%%B', tup.base(inputs[1]))
				:gsub('%%e', tup.ext(inputs[1]))
		end

		command = command:gsub('%%d', tup.getdirectory())

		command = evalGlobals(command)
		command = evalConfig(command)
	end
	if arguments.input and type(arguments.input) == 'table' and arguments.input.extra_inputs then
		for k, v in ipairs(arguments.input.extra_inputs) do
			local newinput = tostring(v)
			newinput = evalGlobals(newinput)
			newinput = evalConfig(newinput)
			table.insert(inputs, newinput)
		end
	end

	--print('Defining tup.frule: ' ..
	--	'inputs (' ..  (type(inputs) == 'table' and table.concat(inputs, ' ') or tostring(inputs)) ..
	--	'), outputs = (' .. (type(outputs) == 'table' and table.concat(outputs, ' ') or tostring(outputs)) ..
	--	'), command = ' .. command)
	tup.definerule{inputs = inputs, outputs = outputs, command = command}

	return outputs
end

local function set_command(str)
	if type(str) != 'string' then
		error 'Expected command to be a string'
	end
	return str
end

local function set_list(t, s)
	if type(t) != 'table' then
		error('Expected ' .. s .. ' to be an array of strings')
	end
	return t
end

local function set_string_or_table(st)
	if type(st) == 'string' then
		return {st}
	else
		return st
	end
end

local function get_abc(a, b, c)
	local inputs = nil
	local command = nil
	local outputs = nil
	if c == nil then
		if b == nil then
			command = set_command(a)
		else
			if type(a) == 'string' and type(b) == 'string' then
				error 'Ambiguous rule: the 2-variable form of this rule must use a table for either the inputs or outputs.'
			end

			if type(a) == 'string' then
				command = set_command(a)
				outputs = set_list(b, 'outputs')
			else
				inputs = set_list(a, 'inputs')
				command = set_command(b)
			end
		end
	else
		inputs = set_string_or_table(a)
		command = set_command(b)
		outputs = set_string_or_table(c)
	end
	return inputs, command, outputs
end

tup.rule = function(a, b, c)
	local input, command, output = get_abc(a, b, c)
	return tup.frule{ input = input, command = command, output = output}
end

tup.foreach_rule = function(a, b, c)
	local input, command, output = get_abc(a, b, c)
	local newinput = {}
	local routputs = {}

	for k, v in ipairs(input) do
		local glob
		glob = conditionalglob(v)
		for globindex, globvalue in ipairs(glob) do
			table.insert(newinput, globvalue)
		end
	end

	for k, v in ipairs(newinput) do
		local tmpi = {v}
		tmpi.extra_inputs = input.extra_inputs
		routputs += tup.frule{input = tmpi, command = command, output = output}
	end
	return routputs
end

-- This function is called when we do 'a += b' in a Tupfile.lua. It works
-- if a and b are strings, tables, or nils, and always makes a result that
-- is an array of strings.
tup_append_assignment = function(a, b)
	local result
	if type(a) == 'string' then
		result = {a}
	elseif type(a) == 'table' then
		result = a
	elseif type(a) == 'nil' then
		result = {}
	else
		error '+= operator only works when the source is a table or string'
	end

	if type(b) == 'string' then
		result[#result+1] = b
	elseif type(b) == 'table' then
		-- append_table in C is almost twice as fast.
		tup.append_table(result, b)
	elseif type(b) == 'nil' then
		-- nothing to do
	else
		error '+= operator only works when the value is a table or string'
	end
	return result
end
