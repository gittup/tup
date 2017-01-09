-- vim: set ts=8 sw=8 sts=8 noet tw=78:
--
-- tup - A file-based build system
--
-- Copyright (C) 2013  Rendaw <rendaw@zarbosoft.com>
-- Copyright (C) 2013-2017  Mike Shal <marfey@gmail.com>
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

local function unchdir_after(...)
	tup.unchdir()
	return ...
end

local realioopen = io.open
function io.open(filename, mode)
	tup.chdir(filename, mode)
	return unchdir_after(realioopen(filename, mode))
end

local realiolines = io.lines
function io.lines(filename, ...)
	tup.chdir(filename)
	return unchdir_after(realiolines(filename, ...))
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

function tableize(inp)
	if type(inp) ~= 'table' then
		return { inp }
	end
	return inp
end

tup.frule = function(arguments)
	if arguments.inputs then
		if type(arguments.inputs) == 'table' then
			if arguments.inputs.extra_inputs then
				arguments.extra_inputs = tableize(arguments.inputs.extra_inputs)
				arguments.inputs["extra_inputs"] = nil
			end
		end
		arguments.inputs = tableize(arguments.inputs)
	end
	if arguments.outputs then
		if type(arguments.outputs) == 'table' then
			if arguments.outputs.extra_outputs then
				arguments.extra_outputs = tableize(arguments.outputs.extra_outputs)
				arguments.outputs["extra_outputs"] = nil
			end
		end
		arguments.outputs = tableize(arguments.outputs)
	end
	rc = tup.definerule(arguments)
	return rc
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
	local inputs, command, outputs = get_abc(a, b, c)
	return tup.frule{ inputs = inputs, command = command, outputs = outputs}
end

tup.foreach_rule = function(a, b, c)
	local inputs, command, outputs = get_abc(a, b, c)
	return tup.frule{ inputs = inputs, command = command, outputs = outputs, foreach = 1}
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
