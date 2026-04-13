classdef calculator < handle
    % CALCULATOR  MATLAB wrapper for the C++ Calculator class via MexForge.
    %
    % Example:
    %   calc = math.calculator("myCalc");
    %   result = calc.add(2, 3);           % 5
    %   result = calc.compute(10, 3, "div"); % 3.333...
    %   data = calc.linspace(0, pi, 100);
    %   sines = calc.apply(data, "sin");
    %   plot(data, sines);
    %   clear calc;   % destructor calls destroy

    properties (Access = private)
        id  % Object handle ID from MexForge ObjectStore
    end

    methods (Static)
        function setLogLevel(level)
            % Set logging level: "trace","debug","info","warn","error","off"
            bindings("__log_level", level);
        end

        function setLogFile(path)
            % Enable file logging
            bindings("__log_file", path);
        end

        function enableMatlabLog(enable)
            % Enable/disable MATLAB console logging
            bindings("__log_matlab", enable);
        end

        function enableBufferedLog(enable)
            % Enable/disable buffered logging (faster, flush manually)
            bindings("__log_buffer", enable);
        end

        function flushLog()
            % Flush buffered log messages to MATLAB console
            bindings("__log_flush");
        end

        function fns = listFunctions()
            % List all registered functions
            fns = bindings("__list_functions");
        end
    end

    methods (Access = public)
        function obj = calculator(name)
            % Create a new Calculator instance
            if nargin < 1
                name = "default";
            end
            obj.id = bindings("create", name);
        end

        function delete(obj)
            % Destroy the Calculator instance
            if ~isempty(obj.id)
                bindings("destroy", obj.id);
            end
        end

        % --- Tier 1: Auto-bound methods ---

        function name = getName(obj)
            name = bindings("get_name", obj.id);
        end

        function val = getMemory(obj)
            val = bindings("get_memory", obj.id);
        end

        function setMemory(obj, val)
            bindings("set_memory", obj.id, val);
        end

        function clearMemory(obj)
            bindings("clear_memory", obj.id);
        end

        function result = add(obj, a, b)
            result = bindings("add", obj.id, a, b);
        end

        function result = multiply(obj, a, b)
            result = bindings("multiply", obj.id, a, b);
        end

        function result = power(obj, base, exp)
            result = bindings("power", obj.id, base, exp);
        end

        % --- Tier 2: Lambda-bound methods ---

        function result = compute(obj, a, b, operation)
            % Compute: "add", "sub", "mul", "div"
            result = bindings("compute", obj.id, a, b, operation);
        end

        function result = linspace(obj, startVal, stopVal, n)
            result = bindings("linspace", obj.id, startVal, stopVal, int32(n));
        end

        % --- Tier 3: Custom-bound methods ---

        function result = apply(obj, data, func)
            % Apply function to array: "sin", "cos", "sqrt", "square"
            result = bindings("apply", obj.id, data, func);
        end
    end
end
