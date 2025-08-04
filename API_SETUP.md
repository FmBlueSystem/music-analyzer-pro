# Configuración de API de Claude/Anthropic

## Pasos para configurar la API:

### 1. Obtener API Key
1. Ve a [console.anthropic.com](https://console.anthropic.com)
2. Crea una cuenta o inicia sesión
3. Ve a "API Keys" en el menú
4. Crea una nueva API key
5. Copia la key (solo se muestra una vez)

### 2. Configurar en el proyecto
1. Copia el archivo de ejemplo:
   ```bash
   cp .env.example .env
   ```

2. Edita el archivo `.env`:
   ```bash
   nano .env
   # o usa tu editor preferido
   ```

3. Reemplaza `your-claude-api-key-here` con tu API key real:
   ```
   ANTHROPIC_API_KEY=sk-ant-api03-xxxxx-xxxxx
   ```

### 3. Verificar configuración
1. Reinicia la aplicación:
   ```bash
   npm start
   ```

2. Intenta analizar un archivo con LLM
3. Si ves un error de API key, verifica tu configuración

## Notas importantes:
- **NUNCA** compartas tu API key
- **NUNCA** subas el archivo `.env` a git (ya está en .gitignore)
- La API key debe empezar con `sk-ant-`
- Cada análisis consume tokens de tu cuenta Anthropic

## Troubleshooting:
- **Error: "API key not configured"**: Asegúrate de haber creado el archivo `.env`
- **Error: "Invalid API key"**: Verifica que copiaste la key completa
- **Error: "Rate limit"**: Espera un poco o mejora tu plan en Anthropic