function generate_signatures(mexFunc, outputDir)
    % GENERATE_SIGNATURES  Generate functionSignatures.json for tab-completion.
    %
    % Creates a JSON file that MATLAB uses for argument auto-completion
    % in the command window and editor.
    %
    % Usage:
    %   mexforge.generate_signatures(@bindings)
    %   mexforge.generate_signatures(@bindings, './matlab')
    %
    % The generated file enables:
    %   - Tab-completion for method names
    %   - Argument name hints in the editor
    %   - Type information tooltips

    if nargin < 2
        outputDir = fileparts(which(func2str(mexFunc)));
        if isempty(outputDir)
            outputDir = pwd;
        end
    end

    % Get all methods
    try
        methods = cellstr(mexFunc("__list_methods"));
    catch
        allFuncs = cellstr(mexFunc("__list_functions"));
        methods = allFuncs(~startsWith(allFuncs, '_'));
        methods = setdiff(methods, {'create', 'destroy'});
    end

    % Build signature struct
    sigs = struct();

    for i = 1:numel(methods)
        name = methods{i};

        % Get argument info
        try
            argInfo = mexFunc("__arg_info", name);
        catch
            argInfo = [];
        end

        % Get description
        try
            desc = mexFunc("__describe", name);
        catch
            desc = '';
        end

        % Build input arguments
        inputs = {};
        for j = 1:numel(argInfo)
            arg = struct();
            arg.name = argInfo(j).name;
            arg.kind = 'required';
            if ~argInfo(j).required
                arg.kind = 'namevalue';
            end

            % Map C++ types to MATLAB types for completion
            switch argInfo(j).type
                case {'double', 'single'}
                    arg.type = {{'numeric'}};
                case {'int32', 'uint32', 'int64', 'uint64'}
                    arg.type = {{'integer'}};
                case {'string', 'char'}
                    arg.type = {{'char', 'string'}};
                case 'logical'
                    arg.type = {{'logical'}};
                case 'struct'
                    arg.type = {{'struct'}};
                otherwise
                    arg.type = {{'numeric'}};
            end

            inputs{end+1} = arg; %#ok<AGROW>
        end

        entry = struct();
        if ~isempty(desc)
            entry.description = desc;
        end
        if ~isempty(inputs)
            entry.inputs = inputs;
        end

        % Sanitize name for struct field (replace invalid chars)
        fieldName = matlab.lang.makeValidName(name);
        sigs.(fieldName) = entry;
    end

    % Write JSON
    jsonText = jsonencode(sigs, 'PrettyPrint', true);
    outFile = fullfile(outputDir, 'functionSignatures.json');
    fid = fopen(outFile, 'w');
    fprintf(fid, '%s', jsonText);
    fclose(fid);

    fprintf('Generated %s with %d method signatures.\n', outFile, numel(methods));
end
