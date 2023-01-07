# Launch from fresh build
timeout 5 ./Forscape -platform offscreen

# Launch with file parameter
timeout 5 ./Forscape ../test/interpreter_scripts/in/root_finding_terse.π -platform offscreen

# Launch with app cache
timeout 5 ./Forscape -platform offscreen

# Success if "timeout" did exit by timing out (code 124)
if [ $? == 124 ]; then
    exit 0
else
    exit 1
fi