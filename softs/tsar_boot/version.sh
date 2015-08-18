#!/bin/sh

n="tsarboot"
v=$(cat VERSION)
t=$(date)
u=${USER-root}
h=$(hostname)

reporev=$(svn info 2>/dev/null)
if [ $? -ne 0 ];
then
    reporev=$(git svn info 2>/dev/null)
fi

reporev=$(echo "$reporev" | awk -F: '$1 == "Last Changed Rev" {print $2}')
printf "%s" \
   "const char versionstr[]=\"$n $v ($u@$h $t) (svn revision $reporev)\n\r\";"
