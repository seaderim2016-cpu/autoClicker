@echo off
echo --- Git Setup Assistant ---
echo.
echo This script will initialize a Git repository and prepare it for GitHub.
echo.

if not exist .git (
    echo Initializing new Git repository...
    git init
) else (
    echo Git repository already initialized.
)

echo.
echo Adding files...
git add .

echo.
echo Committing files...
git commit -m "Initial commit: Kernel AutoClicker with Driver and Client"

echo.
echo Renaming branch to 'main'...
git branch -M main

echo.
echo ---------------------------------------------------------------------
echo IMPORTANT: You need to create a NEW repository on GitHub.com now.
echo Do NOT check "Initialize with README" or "Add .gitignore".
echo Just create an empty repository.
echo ---------------------------------------------------------------------
echo.
set /p REMOTELINK="Paste your GitHub Repository URL here (e.g., https://github.com/Start-0/AutoClicker.git): "

echo.
echo Adding remote origin...
git remote add origin %REMOTELINK%
git remote set-url origin %REMOTELINK%

echo.
echo Pushing to GitHub...
git push -u origin main

echo.
echo Done! Go to your Repository -> Actions tab to see the build running.
pause
