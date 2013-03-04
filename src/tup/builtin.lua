tup.var_metatable = {
        __tostring = function(self)
                return table.concat(self, ' ')
        end,
        __concat = function(first, second)
                if type(first) == 'table' and type(second) == 'table'
                then
                        local output = tup.var{}
                        for index, value in ipairs(first) do table.insert(output, value) end
                        for index, value in ipairs(second) do table.insert(output, value) end
                        return output
                end
                return tostring(first) .. tostring(second)
        end,
        __index = function(this, key)
                if table[key] then return table[key] end
                local out = tup.var {}
                rawset(this, key, out)
                return out
        end
}
tup.var = function(contents)
        if not contents then contents = {} end
        setmetatable(contents, tup.var_metatable)
        return contents
end

tup.file = function(filename)
	-- Returns filename sans preceeding dir/'s
	return string.gsub(filename, '[^/\\]*[/\\]', '')
end

tup.base = function(filename)
	-- Returns filename sans preceeding dir/'s and sans the final . and following characters
	return string.gsub(tup.file(filename), '%.%w*$', '')
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
		for index, input in ipairs(inputs) do
			local newinput = input
			newinput = evalGlobals(newinput)
			newinput = evalConfig(newinput)
			table.insert(newinputs, newinput)
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

			newoutput = newoutput:gsub('%%d', tup.getparent())

			table.insert(newoutputs, newoutput)
		end
		outputs = newoutputs
	end

	local command
	if arguments.command then		
		command = arguments.command

		command = evalGlobals(command)
		command = evalConfig(command)

		local outputreplacement = (outputs and table.concat(outputs, ' '):gsub('%%', '%%%%') or '')
		command = command:gsub('%%o', outputreplacement)

		local inputreplacement = inputs and table.concat(inputs, ' '):gsub('%%', '%%%%') or ''
		command = command:gsub('%%f', inputreplacement)

		if not inputs or #inputs > 1 then
			if command:match('%%b') or command:match('%%B') or command:match('%%e') then
				error 'tup.frule can only use command formatters %b, %B, or %e with exactly one input.'
			end
		else
			command = command
				:gsub('%%b', tup.file(inputs[1]))
				:gsub('%%B', tup.base(inputs[1]))
				:gsub('%%e', tup.ext(inputs[1]))
		end

		command = command:gsub('%%d', tup.getparent())
	end

	--print('Defining tup.frule: ' .. 
	--	'inputs (' ..  (type(inputs) == 'table' and table.concat(inputs, ' ') or tostring(inputs)) .. 
	--	'), outputs = (' .. (type(outputs) == 'table' and table.concat(outputs, ' ') or tostring(outputs)) .. 
	--	'), command = ' .. command)
	tup.definerule{inputs = inputs, outputs = outputs, command = command}

	return outputs
end

tup.rule = function(inputs, command, outputs)
	return tup.frule{ input = inputs, output = outputs, command = command }
end

