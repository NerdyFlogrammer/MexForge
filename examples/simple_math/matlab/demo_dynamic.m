% Demo: Dynamic MexObject — no wrapper class needed
%
% This shows how MexObject eliminates the need for writing
% a MATLAB wrapper class. The C++ bindings define the interface.

%% Create calculator using MexObject
calc = mexforge.MexObject(@bindings, "demo");

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
calc.showHelp("compute");
calc.showHelp("linspace");

%% Batch operations
data = calc.linspace(0, pi, 100);
sines = calc.apply(data, "sin");
plot(data, sines);
title('sin(x) via MexForge');

%% Generate function signatures for tab-completion
mexforge.generate_signatures(@bindings);

%% Clean up (automatic via destructor, but explicit is fine too)
clear calc;
