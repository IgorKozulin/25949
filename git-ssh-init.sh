#!/bin/bash
if [ -z "$SSH_AUTH_SOCK" ]; then
    eval "$(ssh-agent -s)"
else
    echo "ss"
fi
ssh-add ~/.ssh/gitnsu
