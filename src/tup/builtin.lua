tup.tconcat = function(first, second)
	-- Returns a table containing elements of first and second
        local output = {}
        for index, value in ipairs(first) do table.insert(output, value) end
        for index, value in ipairs(second) do table.insert(output, value) end
        return output
end

tup.file = function(filename)
	-- Returns filename sans preceeding dir/'s
	return string.gsub(filename, '[^/\\]*[/\\]', '')
end

tup.base = function(filename)
	-- Returns filename sans preceeding dir/'s and sans the final . and following characters
	return string.gsub(tup.file(filename), '%.%w$', '')
end

tup.ext = function(filename)
	-- Returns the part after the final . in filename
	match = string.match(filename, '%.(%w)$')
	return match and match or ''
end

tup.frule = function(arguments)
	-- Takes inputs, outputs, and commands as in tup.definerule,
	-- additionally accepts, input and output which may be either tables or strings
	-- Replaces $(), @(), and %d in inputs with proper values
	-- Replaces $(), @(), %d, %f, %b, %B in outputs with proper values
	-- Replaces $(), @(), %d, %f, %b, %B, %o in command with proper values
	--
	
	function evalGlobals(raw)
		-- Replace $(VAR) with value of global variable VAR
		local var = raw:match('%$%(([^_]*)%)')
		if var then
			return raw:gsub('%$%(' .. var .. '%)', _G[var] and _G[var] or '')
		end
		return raw
	end

	function evalConfig(raw)
		-- Replace @(VAR) with value of config variable VAR
		local configvar = raw:match('@%([^_]*%)')
		if configvar then
			return raw:gsub('@%(' .. configvar .. '%)', tup.getconfig(configvar))
		end
		return raw
	end

	local inputs
	if arguments.inputs then
		inputs = arguments.inputs
	elseif arguments.input and type(arguments.inputs) ~= 'table' then
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

			if (not inputs or not inputs[1]) and (
				output:match('%%b') or
				output:match('%%B') or
				output:match('%%e')) then
				error 'frule can only use output formatters %b, %B, or %e with exactly one input.'
			end
			newoutput = newoutput
				:gsub('%%b', tup.file(inputs[1]))
				:gsub('%%B', tup.base(inputs[1]))
				:gsub('%%e', tup.ext(inputs[1]))
				:gsub('%%d', tup.getparent())

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

		if (not inputs or #inputs > 1) and (command:match('%%b') or command:match('%%B') or command:match('%%e')) then
			error 'frule can only use command formatters %b, %B, or %e with exactly one input.'
		end
		command = command
			:gsub('%%b', tup.file(inputs[1]))
			:gsub('%%B', tup.base(inputs[1]))
			:gsub('%%e', tup.ext(inputs[1]))
			:gsub('%%d', tup.getparent())
	end

	tup.definerule{inputs = inputs, outputs = outputs, command = command}
end

tup.rule = function(inputs, command, outputs)
	tup.frule{ inputs = inputs, outputs = outputs, command = command }
end

