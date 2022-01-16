if grep --exclude=\*.sh --exclude-dir=Eigen -rnw ${GITHUB_WORKSPACE} -i -e "DO THIS"; then
    printf "\nPlease clean up unfinished \"DO THIS\" tasks before merging to main.\n"
    exit 1
fi