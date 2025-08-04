# Instrucciones de Build de Producción

## Requisitos previos
- Node.js 18+ instalado
- API key de Anthropic configurada en `.env`
- Dependencias instaladas: `npm install`

## Generar builds de producción

### Para todas las plataformas:
```bash
npm run dist
```

### Para plataforma específica:
```bash
# macOS (genera .dmg y .zip)
npm run dist:mac

# Windows (genera installer .exe y portable)
npm run dist:win

# Linux (genera .AppImage y .deb)
npm run dist:linux
```

## Archivos generados
Los builds se generan en la carpeta `dist/`:
- **macOS**: `Music Analyzer Pro-2.2.0.dmg` y `.zip`
- **Windows**: `Music Analyzer Pro Setup 2.2.0.exe` y portable
- **Linux**: `Music-Analyzer-Pro-2.2.0.AppImage` y `.deb`

## Antes de distribuir

### 1. Agregar iconos de aplicación
Necesitas crear iconos en la carpeta `assets/`:
- `icon.icns` - Para macOS (512x512)
- `icon.ico` - Para Windows (256x256)
- `icon.png` - Para Linux (512x512)

Puedes usar herramientas online para convertir un PNG a estos formatos.

### 2. Probar el build
1. Instala/ejecuta la aplicación generada
2. Verifica que el motor C++ carga correctamente
3. Prueba análisis de archivos
4. Verifica que la base de datos funciona

### 3. Configuración de usuario final
Los usuarios necesitarán:
1. Crear su propio archivo `.env`
2. Agregar su propia API key de Anthropic
3. Seguir las instrucciones en `API_SETUP.md`

## Notas importantes
- El addon C++ se compila automáticamente antes del build
- La base de datos SQLite se crea en el primer inicio
- Los archivos `.env` no se incluyen en el build
- El build incluye todas las dependencias necesarias