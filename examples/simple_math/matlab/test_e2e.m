% End-to-end test for the simple_math example.
%
% Run from examples/simple_math/ after building with make():
%   cd examples/simple_math
%   make()
%   cd matlab
%   test_e2e()
%
% All tests call assert() — any failure throws an error with the test name.

function test_e2e()
    addpath(fullfile('..'));                  % bindings MEX file
    addpath(fullfile('..', '..', '..', 'matlab'));  % +mexforge package

    passed = 0;
    failed = 0;

    function run(name, fn)
        try
            fn();
            fprintf('  [PASS] %s\n', name);
            passed = passed + 1;
        catch e
            fprintf('  [FAIL] %s: %s\n', name, e.message);
            failed = failed + 1;
        end
    end

    fprintf('\nEnd-to-end tests: simple_math\n');
    fprintf('MATLAB %s — MEX: %s\n\n', version('-release'), which('bindings'));

    % ---- Raw MEX interface --------------------------------------------------

    run('create and destroy', @() test_lifecycle());
    run('add(2, 3) == 5',     @() test_add());
    run('multiply(4, 5) == 20', @() test_multiply());
    run('power(2, 10) == 1024', @() test_power());
    run('compute add',        @() test_compute_add());
    run('compute sub',        @() test_compute_sub());
    run('compute mul',        @() test_compute_mul());
    run('compute div',        @() test_compute_div());
    run('compute div by zero throws', @() test_div_zero());
    run('get_name',           @() test_get_name());
    run('memory set/get/clear', @() test_memory());
    run('linspace length',    @() test_linspace_length());
    run('linspace values',    @() test_linspace_values());
    run('apply sin',          @() test_apply_sin());
    run('multiple objects isolated', @() test_multiple_objects());

    % ---- Built-in commands --------------------------------------------------

    run('__list_functions returns cell', @() test_list_functions());
    run('__store_size',       @() test_store_size());
    run('__store_clear',      @() test_store_clear());
    run('__describe',         @() test_describe());
    run('__return_type',      @() test_return_type());
    run('__arg_info',         @() test_arg_info());
    run('__list_methods',     @() test_list_methods());

    % ---- MexObject dynamic wrapper ------------------------------------------

    run('MexObject create',   @() test_mexobj_create());
    run('MexObject add',      @() test_mexobj_add());
    run('MexObject availableMethods', @() test_mexobj_methods());
    run('MexObject showHelp', @() test_mexobj_help());
    run('MexObject auto-destroy', @() test_mexobj_destroy());

    % ---- Summary ------------------------------------------------------------

    fprintf('\n%d passed, %d failed\n\n', passed, failed);
    if failed > 0
        error('test_e2e:failure', '%d test(s) failed.', failed);
    end
end

% =============================================================================
% Raw MEX tests
% =============================================================================

function test_lifecycle()
    id = bindings("create", "myCalc");
    assert(isnumeric(id) && id >= 0, 'create returned invalid id');
    bindings("destroy", id);
end

function test_add()
    id = bindings("create", "c");
    result = bindings("add", id, 2.0, 3.0);
    bindings("destroy", id);
    assert(abs(result - 5.0) < 1e-10, sprintf('expected 5, got %g', result));
end

function test_multiply()
    id = bindings("create", "c");
    result = bindings("multiply", id, 4.0, 5.0);
    bindings("destroy", id);
    assert(abs(result - 20.0) < 1e-10);
end

function test_power()
    id = bindings("create", "c");
    result = bindings("power", id, 2.0, 10.0);
    bindings("destroy", id);
    assert(abs(result - 1024.0) < 1e-10);
end

function test_compute_add()
    id = bindings("create", "c");
    result = bindings("compute", id, 10.0, 3.0, "add");
    bindings("destroy", id);
    assert(abs(result - 13.0) < 1e-10);
end

function test_compute_sub()
    id = bindings("create", "c");
    result = bindings("compute", id, 10.0, 3.0, "sub");
    bindings("destroy", id);
    assert(abs(result - 7.0) < 1e-10);
end

function test_compute_mul()
    id = bindings("create", "c");
    result = bindings("compute", id, 6.0, 7.0, "mul");
    bindings("destroy", id);
    assert(abs(result - 42.0) < 1e-10);
end

function test_compute_div()
    id = bindings("create", "c");
    result = bindings("compute", id, 10.0, 4.0, "div");
    bindings("destroy", id);
    assert(abs(result - 2.5) < 1e-10);
end

function test_div_zero()
    id = bindings("create", "c");
    threw = false;
    try
        bindings("compute", id, 1.0, 0.0, "div");
    catch
        threw = true;
    end
    bindings("destroy", id);
    assert(threw, 'division by zero should throw');
end

function test_get_name()
    id = bindings("create", "MyCalc");
    name = bindings("get_name", id);
    bindings("destroy", id);
    assert(strcmp(name, "MyCalc"), sprintf('expected MyCalc, got %s', name));
end

function test_memory()
    id = bindings("create", "c");
    bindings("set_memory", id, 3.14);
    val = bindings("get_memory", id);
    assert(abs(val - 3.14) < 1e-10);
    bindings("clear_memory", id);
    val = bindings("get_memory", id);
    assert(abs(val) < 1e-10, 'memory should be 0 after clear');
    bindings("destroy", id);
end

function test_linspace_length()
    id = bindings("create", "c");
    v = bindings("linspace", id, 0.0, 1.0, int32(11));
    bindings("destroy", id);
    assert(numel(v) == 11, sprintf('expected 11 elements, got %d', numel(v)));
end

function test_linspace_values()
    id = bindings("create", "c");
    v = bindings("linspace", id, 0.0, 10.0, int32(6));
    bindings("destroy", id);
    expected = [0 2 4 6 8 10];
    assert(max(abs(v - expected)) < 1e-10, 'linspace values wrong');
end

function test_apply_sin()
    id = bindings("create", "c");
    x = bindings("linspace", id, 0.0, pi, int32(5));
    y = bindings("apply", id, x, "sin");
    bindings("destroy", id);
    assert(numel(y) == 5, 'apply should return same length');
    assert(abs(y(1)) < 1e-10,     'sin(0) should be ~0');
    assert(abs(y(end)) < 1e-10,   'sin(pi) should be ~0');
    assert(abs(y(3) - 1.0) < 1e-5, 'sin(pi/2) should be ~1');
end

function test_multiple_objects()
    id1 = bindings("create", "A");
    id2 = bindings("create", "B");
    bindings("set_memory", id1, 10.0);
    bindings("set_memory", id2, 20.0);
    v1 = bindings("get_memory", id1);
    v2 = bindings("get_memory", id2);
    bindings("destroy", id1);
    bindings("destroy", id2);
    assert(abs(v1 - 10.0) < 1e-10 && abs(v2 - 20.0) < 1e-10, ...
        'objects should be isolated');
end

% =============================================================================
% Built-in command tests
% =============================================================================

function test_list_functions()
    fns = cellstr(bindings("__list_functions"));
    assert(iscell(fns) && numel(fns) > 0, 'expected non-empty cell array');
    assert(any(strcmp(fns, 'add')), 'add should be in list');
    assert(any(strcmp(fns, 'create')), 'create should be in list');
end

function test_store_size()
    bindings("__store_clear");
    sz = bindings("__store_size");
    assert(sz == 0, 'store should be empty after clear');
    id = bindings("create", "x");
    sz = bindings("__store_size");
    assert(sz == 1, 'store size should be 1 after create');
    bindings("destroy", id);
end

function test_store_clear()
    id1 = bindings("create", "a"); %#ok<NASGU>
    id2 = bindings("create", "b"); %#ok<NASGU>
    bindings("__store_clear");
    sz = bindings("__store_size");
    assert(sz == 0, 'store should be 0 after __store_clear');
end

function test_describe()
    desc = bindings("__describe", "add");
    assert(~isempty(desc), 'add should have a description');
    assert(contains(desc, "Add") || contains(desc, "add") || contains(desc, "two"), ...
        sprintf('unexpected description: %s', desc));
end

function test_return_type()
    rt = bindings("__return_type", "add");
    assert(strcmp(rt, "double"), sprintf('expected double, got %s', rt));
    rt = bindings("__return_type", "create");
    assert(strcmp(rt, "uint32"), sprintf('expected uint32, got %s', rt));
end

function test_arg_info()
    info = bindings("__arg_info", "add");
    assert(isstruct(info), '__arg_info should return struct array');
    assert(numel(info) == 2, 'add has 2 args');
    assert(strcmp(info(1).name, 'a'), sprintf('first arg name: %s', info(1).name));
    assert(strcmp(info(1).type, 'double'), 'first arg type should be double');
    assert(info(1).required == true, 'first arg should be required');
end

function test_list_methods()
    methods = cellstr(bindings("__list_methods"));
    assert(iscell(methods) && numel(methods) > 0);
    assert(any(strcmp(methods, 'add')));
    assert(~any(strcmp(methods, 'create')), 'create is free, not a method');
end

% =============================================================================
% MexObject tests
% =============================================================================

function test_mexobj_create()
    calc = mexforge.MexObject(@bindings, "test");
    assert(~isempty(calc));
    clear calc;
end

function test_mexobj_add()
    calc = mexforge.MexObject(@bindings, "test");
    result = calc.add(7.0, 8.0);
    assert(abs(result - 15.0) < 1e-10, sprintf('expected 15, got %g', result));
    clear calc;
end

function test_mexobj_methods()
    calc = mexforge.MexObject(@bindings, "test");
    methods = calc.availableMethods();
    assert(iscell(methods) && numel(methods) > 0);
    assert(any(strcmp(methods, 'add')));
    clear calc;
end

function test_mexobj_help()
    calc = mexforge.MexObject(@bindings, "test");
    % showHelp should not throw
    calc.showHelp("add");
    clear calc;
end

function test_mexobj_destroy()
    calc = mexforge.MexObject(@bindings, "test");  %#ok<NASGU>
    sz_before = bindings("__store_size");
    clear calc;
    sz_after = bindings("__store_size");
    assert(sz_after == sz_before - 1, 'MexObject destructor should call destroy');
end
