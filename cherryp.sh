#!/bin/bash
git rev-list --reverse b740d2de7ad75360898ec6769ed595b81340ad19^..010c2801a05eade6815303e368a6784930a51702 | while read rev  
do 
git cherry-pick $rev || break
done
