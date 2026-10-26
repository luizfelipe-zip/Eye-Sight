# Remove o Eye Sight instalado, a assinatura local e a inicialização automática.
$subject = 'CN=Eye Sight (assinatura local)'
Get-Process EyeSight -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 700
Remove-Item (Join-Path $env:ProgramFiles 'Eye Sight') -Recurse -Force -ErrorAction SilentlyContinue
foreach ($store in 'Root', 'TrustedPublisher', 'My') {
    Get-ChildItem "Cert:\LocalMachine\$store" | Where-Object Subject -eq $subject | Remove-Item -ErrorAction SilentlyContinue
}
Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name EyeSight -ErrorAction SilentlyContinue
Write-Host 'Eye Sight removido.' -ForegroundColor Green
Read-Host 'Pressione Enter para fechar'
