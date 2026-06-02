# EventHorizon Benchmark Runner (PowerShell)
# Usage: .\run.ps1 [bench|test|neuro|clean|help]

param(
    [string]$Command = "bench"
)

$ProjectPath = "/mnt/c/Users/Administrator/Desktop/my_project/eventhorizon"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  EventHorizon Engine - Quick Runner" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

switch ($Command) {
    "bench" {
        Write-Host "Running benchmark..." -ForegroundColor Green
        wsl bash -c "cd $ProjectPath; make run-bench"
    }
    "test" {
        Write-Host "Running tests..." -ForegroundColor Green
        wsl bash -c "cd $ProjectPath; make run-test"
    }
    "neuro" {
        Write-Host "Running neuro test..." -ForegroundColor Green
        wsl bash -c "cd $ProjectPath; make run-neuro"
    }
    "clean" {
        Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
        wsl bash -c "cd $ProjectPath; make clean"
    }
    "build" {
        Write-Host "Building all targets..." -ForegroundColor Green
        wsl bash -c "cd $ProjectPath; make build-all"
    }
    "help" {
        Write-Host "Available commands:" -ForegroundColor Yellow
        Write-Host "  .\run.ps1 bench   - Run benchmark suite (default)"
        Write-Host "  .\run.ps1 test    - Run test suite"
        Write-Host "  .\run.ps1 neuro   - Run neuro test"
        Write-Host "  .\run.ps1 build   - Build all targets"
        Write-Host "  .\run.ps1 clean   - Clean build artifacts"
        Write-Host "  .\run.ps1 help    - Show this help"
    }
    default {
        Write-Host "Unknown command: $Command" -ForegroundColor Red
        Write-Host "Run .\run.ps1 help for available commands"
    }
}
