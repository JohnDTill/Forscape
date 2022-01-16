if grep --exclude=\*.sh -rnw ${GITHUB_WORKSPACE} -i -e "DO THIS"; then
    echo "Please clean up unfinished tasks before merging to main."
    exit 1
fi