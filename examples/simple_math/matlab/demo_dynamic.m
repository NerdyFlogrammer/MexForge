% Demo: Dynamic MexObject — no wrapper class needed
%
% This shows how MexObject eliminates the need for writing
% a MATLAB wrapper class. The C++ bindings define the interface.
%
% For full editor tab-completion, run once:
%   mexforge.generate_signatures(@bindings)
% Then use the generated class:
%   calc = bindings_obj("demo");

%% Setup: add MexForge's MATLAB package and bindings directory to the path
repoRoot    = fullfile(fileparts(mfilename('fullpath')), '..', '..', '..');
bindingsDir = fullfile(fileparts(mfilename('fullpath')), '..');
addpath(fullfile(repoRoot, 'matlab'));   % +mexforge package
addpath(bindingsDir);                    % bindings MEX + generated class

%% Generate wrapper class for full editor tab-completion
%  (run once after each recompile — creates bindings_obj.m in bindingsDir)
fprintf('Generating tab-completion files...\n');
mexforge.generate_signatures(@bindings, bindingsDir);
rehash path;   % let MATLAB pick up the newly written bindings_obj.m

%% Create calculator using the auto-generated wrapper class
%  The editor now knows all methods — tab-completion works!
calc = bindings_obj("demo");



%% Call methods dynamically — these dispatch to C++ automatically
result = calc.add(2, 3);
fprintf('add(2, 3) = %g\n', result);

result = calc.multiply(4, 5);
fprintf('multiply(4, 5) = %g\n', result);

result = calc.power(2, 10);
fprintf('power(2, 10) = %g\n', result);

result = calc.compute(10, 3, "div");
fprintf('compute(10, 3, "div") = %g\n', result);

%% Introspection — see what's available
calc.availableMethods();

%% Help for a specific method
calc.help("compute");
calc.help("linspace");
%% Batch operations
data = calc.linspace(0, pi, 100);
sines = calc.apply(data, "sin");
plot(data, sines);
title('sin(x) via MexForge');

%% Clean up (automatic via destructor, but explicit is fine too)
clear calc;
