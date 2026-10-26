# Instala o Eye Sight em "Arquivos de Programas" com assinatura local, para a textura
# ficar por cima do menu Iniciar e da barra de tarefas (recurso uiAccess do Windows).
# O Windows só libera esse recurso para programas assinados e instalados em pasta protegida.
$ErrorActionPreference = 'Stop'
$subject = 'CN=Eye Sight (assinatura local)'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $here 'EyeSight-uiaccess.exe'
$dir = Join-Path $env:ProgramFiles 'Eye Sight'
$dst = Join-Path $dir 'EyeSight.exe'

try {
    Write-Host 'Fechando o Eye Sight, se estiver aberto...'
    Get-Process EyeSight* -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 700

    Write-Host "Copiando para $dir"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Copy-Item $src $dst -Force

    Write-Host 'Criando a assinatura local...'
    foreach ($store in 'Root', 'TrustedPublisher', 'My') {
        Get-ChildItem "Cert:\LocalMachine\$store" | Where-Object Subject -eq $subject | Remove-Item -ErrorAction SilentlyContinue
    }
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject `
        -CertStoreLocation Cert:\LocalMachine\My -KeyExportPolicy NonExportable -NotAfter (Get-Date).AddYears(20)
    $cer = Join-Path $env:TEMP 'eyesight.cer'
    Export-Certificate -Cert $cert -FilePath $cer | Out-Null
    Import-Certificate -FilePath $cer -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
    Import-Certificate -FilePath $cer -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null
    Remove-Item $cer

    $sig = Set-AuthenticodeSignature -FilePath $dst -Certificate $cert -HashAlgorithm SHA256
    # apaga a chave privada: depois de assinar o Eye Sight, ela não serve para assinar mais nada
    Remove-Item -Path "Cert:\LocalMachine\My\$($cert.Thumbprint)" -DeleteKey
    if ($sig.Status -ne 'Valid') { throw "A assinatura falhou: $($sig.StatusMessage)" }

    # inicialização com o Windows (o programa instalado nem sempre consegue gravar isso sozinho)
    Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name EyeSight -Value "`"$dst`" --tray"
    if (-not (Test-Path 'HKCU:\Software\EyeSight')) { New-Item 'HKCU:\Software\EyeSight' | Out-Null }
    New-ItemProperty 'HKCU:\Software\EyeSight' -Name Startup -Value 1 -PropertyType DWord -Force | Out-Null

    Write-Host 'Abrindo o Eye Sight...'
    # abre pelo Explorer para rodar como usuário comum (não como administrador)
    Start-Process explorer.exe -ArgumentList "`"$dst`""
    Write-Host ''
    Write-Host 'Pronto! O Eye Sight está instalado e inicia junto com o Windows.' -ForegroundColor Green
} catch {
    Write-Host ''
    Write-Host "Erro: $($_.Exception.Message)" -ForegroundColor Red
}
Read-Host 'Pressione Enter para fechar'
