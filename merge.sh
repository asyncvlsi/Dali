#!/bin/bash

git checkout develop
git merge -s ours --no-commit master
git commit -m "replace master by develop"
git checkout master
git merge develop
