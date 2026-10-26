# Remove o Eye Sight instalado, a assinatura local e a inicialização automática,
# e devolve os cursores normais.
$subject = 'CN=Eye Sight (assinatura local)'

# Pede para o Eye Sight sair (assim ele devolve os cursores e as cores normais). Se ele não
# fechar em 3 s, ou for uma versão antiga sem --exit, encerra à força.
function Close-EyeSight {
    $exe = Join-Path $env:ProgramFiles 'Eye Sight\EyeSight.exe'
    $graceful = $false
    try { $graceful = [version](Get-Item $exe).VersionInfo.FileVersion -ge [version]'1.2.0' } catch {}
    if ($graceful -and (Get-Process EyeSight* -ErrorAction SilentlyContinue)) {
        Start-Process $exe -ArgumentList '--exit'
        for ($i = 0; $i -lt 30 -and (Get-Process EyeSight* -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 100 }
    }
    Get-Process EyeSight* -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 700
    # recarrega os cursores do usuário, caso o programa tenha sido encerrado à força
    Add-Type -Namespace EyeSight -Name Native -MemberDefinition '[DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint a, uint b, System.IntPtr c, uint d);'
    [EyeSight.Native]::SystemParametersInfo(0x57, 0, [System.IntPtr]::Zero, 0) | Out-Null
}

Close-EyeSight
Remove-Item (Join-Path $env:ProgramFiles 'Eye Sight') -Recurse -Force -ErrorAction SilentlyContinue
foreach ($store in 'Root', 'TrustedPublisher', 'My') {
    Get-ChildItem "Cert:\LocalMachine\$store" | Where-Object Subject -eq $subject | Remove-Item -ErrorAction SilentlyContinue
}
Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name EyeSight -ErrorAction SilentlyContinue
Write-Host 'Eye Sight removido.' -ForegroundColor Green
Read-Host 'Pressione Enter para fechar'
