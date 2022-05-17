#!bin/bash

git checkout main
git branch -d develop
git push origin --delete develop

