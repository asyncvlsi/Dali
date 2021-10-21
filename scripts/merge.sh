#!/bin/bash

# merge develop to main and push the new main branch to GitHub
git checkout develop
git merge -s ours --no-commit main
git commit -m "replace main by develop"
git checkout main
git merge develop
git push
