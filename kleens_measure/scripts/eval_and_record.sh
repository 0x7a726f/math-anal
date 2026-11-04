#!/usr/bin/env bash
# Atomic Evaluation & Auto-Rollback Script for KMP Research Station
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." >/dev/null 2>&1 && pwd )"
cd "$DIR"
python scripts/eval_and_record.py "$@"
