classdef MexObject < handle & matlab.mixin.CustomDisplay
    % MEXOBJECT  Dynamic MATLAB wrapper for any MexForge-based MEX library.
    %
    % Automatically exposes all registered C++ functions as MATLAB methods.
    % No per-method wrapper code needed — the C++ bindings ARE the interface.
    %
    % Features:
    %   - Dynamic method dispatch via subsref
    %   - Tab-completion for all registered methods
    %   - Built-in help texts from C++ metadata
    %   - Argument type checking before MEX call
    %
    % Usage:
    %   calc = mexforge.MexObject(@bindings, "myCalc");
    %   calc.add(2, 3)
    %   calc.power(2, 10)
    %   calc.help("add")
    %   calc.methods()
    %
    % See also: mexforge.generate_signatures

    properties (Access = private)
        id_          % Object handle ID from MexForge ObjectStore
        mexFunc_     % Function handle to the MEX entry point
        methods_     % Cell array of available object method names
        methodSet_   % containers.Map for fast lookup
    end

    methods (Access = public)

        function obj = MexObject(mexFunc, varargin)
            % MEXOBJECT  Create a new MexForge object.
            %   obj = mexforge.MexObject(@mex_function, constructor_args...)
            %
            %   mexFunc  - Function handle to the compiled MEX file
            %   varargin - Arguments passed to the "create" function

            obj.mexFunc_ = mexFunc;
            obj.id_ = mexFunc("create", varargin{:});

            % Cache available methods
            try
                obj.methods_ = cellstr(mexFunc("__list_methods"));
            catch
                % Fallback: use __list_functions and filter
                allFuncs = cellstr(mexFunc("__list_functions"));
                obj.methods_ = allFuncs(~startsWith(allFuncs, '_'));
                obj.methods_ = setdiff(obj.methods_, {'create', 'destroy'});
            end

            % Build lookup map for O(1) method checking
            obj.methodSet_ = containers.Map(obj.methods_, ...
                true(1, numel(obj.methods_)));
        end

        function delete(obj)
            % Destructor — releases the C++ object
            if ~isempty(obj.id_) && ~isempty(obj.mexFunc_)
                try
                    obj.mexFunc_("destroy", obj.id_);
                catch
                    % Suppress errors during MATLAB shutdown
                end
            end
        end

        % ---- Introspection ----

        function list = availableMethods(obj)
            % AVAILABLEMETHODS  List all available methods on this object.
            list = sort(obj.methods_);
            if nargout == 0
                fprintf('\nAvailable methods:\n');
                for i = 1:numel(list)
                    desc = obj.getDescription(list{i});
                    if isempty(desc)
                        fprintf('  %s\n', list{i});
                    else
                        fprintf('  %-25s  %s\n', list{i}, desc);
                    end
                end
                fprintf('\n');
                clear list;
            end
        end

        function help(obj, methodName)
            % HELP  Show help for a specific method.
            %   obj.help("add")
            obj.showHelp(methodName);
        end

        function showHelp(obj, methodName)
            % SHOWHELP  Show help for a specific method (alias: obj.help).
            %   obj.showHelp("add")

            desc = obj.getDescription(methodName);
            fprintf('\n  %s', methodName);

            if ~isempty(desc)
                fprintf(' — %s', desc);
            end
            fprintf('\n');

            % Show arguments
            try
                argInfo = obj.mexFunc_("__arg_info", methodName);
                if ~isempty(argInfo)
                    fprintf('\n  Arguments:\n');
                    for i = 1:numel(argInfo)
                        if argInfo(i).required
                            reqStr = 'required';
                        else
                            reqStr = 'optional';
                        end
                        fprintf('    %-15s  %-10s  (%s)\n', ...
                            argInfo(i).name, argInfo(i).type, reqStr);
                    end
                end
            catch
                % No arg info available
            end

            % Show return type
            try
                retType = obj.mexFunc_("__return_type", methodName);
                if ~isempty(retType) && ~strcmp(retType, 'void')
                    fprintf('\n  Returns: %s\n', retType);
                end
            catch
                % No return type available
            end

            fprintf('\n');
        end

        function checkArgs(obj, methodName, varargin)
            % CHECKARGS  Validate arguments before calling a method.
            %   obj.checkArgs("add", 2, 3)

            try
                argInfo = obj.mexFunc_("__arg_info", methodName);
            catch
                return;  % No metadata, skip validation
            end

            if isempty(argInfo)
                return;
            end

            nRequired = sum([argInfo.required]);
            nTotal = numel(argInfo);
            nProvided = numel(varargin);

            if nProvided < nRequired
                error('mexforge:tooFewArgs', ...
                    '%s requires at least %d argument(s), got %d.', ...
                    methodName, nRequired, nProvided);
            end

            if nProvided > nTotal
                error('mexforge:tooManyArgs', ...
                    '%s accepts at most %d argument(s), got %d.', ...
                    methodName, nTotal, nProvided);
            end

            % Type check each provided argument
            typeMap = containers.Map( ...
                {'double', 'single', 'int32', 'uint32', 'int64', 'uint64', ...
                 'logical', 'bool', 'string', 'struct'}, ...
                {'double', 'single', 'int32', 'uint32', 'int64', 'uint64', ...
                 'logical', 'logical', 'string', 'struct'});

            for i = 1:nProvided
                expectedType = argInfo(i).type;
                if typeMap.isKey(expectedType)
                    matlabType = typeMap(expectedType);
                    actual = class(varargin{i});

                    % Allow numeric coercion (double -> int32, etc.)
                    if strcmp(matlabType, 'string') && ~isStringOrChar(varargin{i})
                        error('mexforge:typeMismatch', ...
                            '%s: argument ''%s'' expects %s, got %s.', ...
                            methodName, argInfo(i).name, expectedType, actual);
                    end
                end
            end
        end
    end

    % ---- Dynamic dispatch via subsref ----

    methods (Hidden)
        function varargout = subsref(obj, s)
            if numel(s) >= 2 && strcmp(s(1).type, '.') && strcmp(s(2).type, '()')
                methodName = string(s(1).subs);  % s(1).subs is char; MEX expects string

                % Check if this is a registered MEX method
                if obj.methodSet_.isKey(methodName)
                    args = s(2).subs;

                    % Optional: validate arguments
                    obj.checkArgs(methodName, args{:});

                    % Call MEX with object ID
                    [varargout{1:nargout}] = obj.mexFunc_(methodName, obj.id_, args{:});

                    % Handle remaining subscript operations (e.g., obj.method().field)
                    if numel(s) > 2
                        [varargout{1:nargout}] = builtin('subsref', varargout{1}, s(3:end));
                    end
                    return;
                end
            end

            % Property access or built-in methods
            if numel(s) == 1 && strcmp(s(1).type, '.')
                methodName = string(s(1).subs);  % s(1).subs is char; MEX expects string
                if obj.methodSet_.isKey(methodName)
                    % Method referenced without () — return function handle
                    varargout{1} = @(varargin) obj.callMethod(methodName, varargin{:});
                    return;
                end
            end

            % Fallback to built-in
            [varargout{1:nargout}] = builtin('subsref', obj, s);
        end
    end

    % ---- Tab-completion ----
    % Returning the MEX method names from properties() makes MATLAB's dot-notation
    % tab completion (obj.<TAB>) show the available methods in the command window.
    % fieldnames() is used as a fallback for struct-like expansion contexts.

    methods (Hidden, Access = public)
        function names = properties(obj)
            names = sort(obj.methods_);
        end

        function names = fieldnames(obj)
            names = sort(obj.methods_);
        end
    end

    % ---- Custom display ----

    methods (Access = protected)
        function displayScalarObject(obj)
            className = class(obj);
            header = matlab.mixin.CustomDisplay.getClassNameForHeader(obj);
            fprintf('  %s (MexForge object, ID=%d)\n\n', header, obj.id_);
            fprintf('  %d methods available. Use obj.availableMethods() to list, obj.help(name) for docs.\n\n', ...
                numel(obj.methods_));
        end
    end

    % ---- Private helpers ----

    methods (Access = private)
        function [varargout] = callMethod(obj, name, varargin)
            obj.checkArgs(name, varargin{:});
            [varargout{1:nargout}] = obj.mexFunc_(name, obj.id_, varargin{:});
        end

        function desc = getDescription(obj, methodName)
            try
                desc = obj.mexFunc_("__describe", methodName);
            catch
                desc = '';
            end
        end
    end
end

function result = isStringOrChar(val)
    result = ischar(val) || isstring(val);
end
