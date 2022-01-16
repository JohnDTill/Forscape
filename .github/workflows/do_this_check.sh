if grep --exclude=\*.sh -rnw ${GITHUB_WORKSPACE} -i -e "DO THIS"; then
    exit 1
fi