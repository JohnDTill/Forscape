exc={${GITHUB_WORKSPACE}/src/Eigen, ${GITHUB_WORKSPACE}/src/unsupported}

if grep --exclude=\*.sh --exclude-dir=${exc} -rnw ${GITHUB_WORKSPACE} -i -e "DO THIS"; then
    echo "\nPlease clean up unfinished \"DO THIS\" tasks before merging to main.\n"
    exit 1
fi