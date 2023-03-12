using System.Collections;
using Nanoforge.App;
using System.IO;
using Direct3D;
using Common;
using System;
using Win32;
using System.Text;
using Nanoforge.Misc;
using System.Threading;

namespace Nanoforge.Render.Resources
{
	public class Shader
	{
        private ID3D11Device* _device = null;
        private String _shaderPath = new .() ~delete _;
        private DateTime _fileWriteTime;
        private bool _useGeometryShaders = false;

        private ID3D11VertexShader* _vertexShader = null ~ReleaseCOM(&_);
        private ID3D11PixelShader* _pixelShader = null ~ReleaseCOM(&_);
        private ID3D11GeometryShader* _geometryShader = null ~ReleaseCOM(&_);
        private ID3DBlob* _vsBlob = null ~ReleaseCOM(&_); //This blob is preserved since the material needs it for vertex input layout setup

        //Track list of included files + their write times so hot reload works when they're changed too
        private List<String> _includePaths = new .() ~DeleteContainerAndItems!(_);
        private List<DateTime> _includeFileWriteTimes = new .() ~delete _;

        public ID3DBlob* GetVertexShaderBlob() => _vsBlob;

        //Load and compile shader source file (.hlsl)
        public Result<void> Load(StringView name, ID3D11Device* device, bool useGeometryShaders)
        {
            _shaderPath.Set(scope $"{BuildConfig.ShadersPath}{name}");
            _device = device;
            _fileWriteTime = File.GetLastWriteTime(_shaderPath);
            _useGeometryShaders = useGeometryShaders;

            ClearAndDeleteItems!(_includePaths);
            _includeFileWriteTimes.Clear();

            ID3D11VertexShader* vertexShader = null;
            ID3DBlob* vsBlob = null;
            ID3D11PixelShader* pixelShader = null;
            ID3DBlob* psBlob = null;
            ID3D11GeometryShader* geometryShader = null;
            ID3DBlob* gsBlob = null;

            //Compile vertex shader
            if (CompileFromFile("VS", "vs_4_0", &vsBlob) case .Err)
                return .Err;
            if (FAILED!(device.CreateVertexShader(vsBlob.GetBufferPointer(), vsBlob.GetBufferSize(), null, &vertexShader)))
                return .Err;

            //Compile pixel shader
            if (CompileFromFile("PS", "ps_4_0", &psBlob) case .Err)
                return .Err;
            if (FAILED!(device.CreatePixelShader(psBlob.GetBufferPointer(), psBlob.GetBufferSize(), null, &pixelShader)))
                return .Err;

            //Compile geometry shader
            if (_useGeometryShaders)
            {
                if (CompileFromFile("GS", "gs_4_0", &gsBlob) case .Err)
                    return .Err;
                if (FAILED!(device.CreateGeometryShader(gsBlob.GetBufferPointer(), gsBlob.GetBufferSize(), null, &geometryShader)))
                    return .Err;
            }

            //Release old copies of shader data
            ReleaseCOM(&_vertexShader);
            ReleaseCOM(&_pixelShader);
            ReleaseCOM(&_geometryShader);
            ReleaseCOM(&_vsBlob);

            //Store newly compiled shader data
            _vertexShader = vertexShader;
            _pixelShader = pixelShader;
            _vsBlob = vsBlob;
            if (_useGeometryShaders)
            {
                _geometryShader = geometryShader;
            }
            ReleaseCOM(&psBlob);
            ReleaseCOM(&gsBlob);
            Logger.Info("Loaded '{}'", Path.GetFileName(_shaderPath, .. scope .()));

            return .Ok;
        }

        public void TryReload()
        {
            if (NeedsReload())
            {
                String shaderFileName = Path.GetFileName(_shaderPath, .. scope .());
                Logger.Info("Reloading '{}'", shaderFileName);
                Thread.Sleep(250); //Wait a moment to make sure the shader isn't being saved by another process while we're loading it. Stupid fix, but it works.
                if (Load(shaderFileName, _device, _useGeometryShaders) case .Err)
                {
                    Logger.Error("Failed to reload {}", shaderFileName);
                }
            }
        }

        //Returns true if the shader source file or any dependencies have changed since it was last loaded
        public bool NeedsReload()
        {
            //Has the primary shader file changed?
            if (_fileWriteTime != File.GetLastWriteTime(_shaderPath))
                return true;

            //Have any of the included files changed?
            for (int i in 0..<_includePaths.Count)
                if (_includeFileWriteTimes[i] != File.GetLastWriteTime(_includePaths[i]))
                    return true;

            return false;
        }

        public void Bind(ID3D11DeviceContext* context)
        {
            context.VSSetShader(_vertexShader, null, 0);
            context.PSSetShader(_pixelShader, null, 0);
            if (_useGeometryShaders)
                context.GSSetShader(_geometryShader, null, 0);
            else
                context.GSSetShader(null, null, 0);
        }

        //Compile shader from hlsl file. Compiles only one shader type per call with unified shaders (pixel/geom/vertex all in one file)
        private Result<void> CompileFromFile(char8* entryPoint, char8* shaderModel, ID3DBlob** blobOut)
        {
            using (ShaderIncludeHandler includeHandler = .(_includePaths, _includeFileWriteTimes))
            {
                u32 shaderFlags = Direct3D.D3DCOMPILE_ENABLE_STRICTNESS;
                if (BuildConfig.EnableD3D11DebugFeatures)
                {
                    shaderFlags |= Direct3D.D3DCOMPILE_DEBUG;
                    shaderFlags |= Direct3D.D3DCOMPILE_SKIP_OPTIMIZATION;
                }

                //Convert path to wide string
                char16[] pathWide = scope char16[UTF16.GetEncodedLen(_shaderPath) + 1];
                if (UTF16.Encode(_shaderPath, pathWide.CArray(), pathWide.Count) case .Err(let err))
                {
                    Logger.Error("Encoding error '{}' while converting shader path to UTF16", err);
					return .Err;
                }
                pathWide[pathWide.Count - 1] = '\0';

                //Compile shader
                ID3DBlob* errorBlob = null;
                i32 result = Direct3D.D3DCompileFromFile(pathWide.CArray(), null, &includeHandler, entryPoint, shaderModel, shaderFlags, 0, blobOut, &errorBlob);
                if (FAILED!(result))
                {
                    if (errorBlob != null)
                    {
                        StringView error = .((char8*)errorBlob.GetBufferPointer(), (int)errorBlob.GetBufferSize());
                        Logger.Error("Shader compiler error: '{}'", error);
                        errorBlob.Release();
                        return .Err;
                    }
                    else
                    {
                        Logger.Error("Shader compiler error. No error provided by compiler.");
                        return .Err;
                    }
                }
                ReleaseCOM(&errorBlob);
            }

            return .Ok;
        }

        //CompileShaderFromPath passes any #include instances from shaders to this. Used to make a list of shader dependencies for hot reload
        //Dispose must be called on this for all allocations to be safely destroyed
        private struct ShaderIncludeHandler : ID3DInclude, IDisposable
        {
            public List<String> IncludePaths = null;
            public List<DateTime> IncludeWriteTimes = null;
            private List<String> _fileStrings = new .();

            public this(List<String> includePaths, List<DateTime> includeWriteTimes)
            {
                IncludePaths = includePaths;
                IncludeWriteTimes = includeWriteTimes;
                vt = new ID3DInclude.VTable();
                vt.Open = => ShaderIncludeHandler.Open;
                vt.Close = => ShaderIncludeHandler.Close;
            }

            void IDisposable.Dispose()
            {
                if (vt != null)
                    delete vt;
                DeleteContainerAndItems!(_fileStrings);
            }

            [CallingConvention(.Stdcall)]
            public static HRESULT Open(ID3DInclude* self, D3D_INCLUDE_TYPE IncludeType, char8* pFileName, void* pParentData, void** ppData, out uint32 pBytes)
            {
                ShaderIncludeHandler* selfTyped = (ShaderIncludeHandler*)self;

                //Record shader path + write time for live reload tracking
                String includePath = new $"{BuildConfig.ShadersPath}"..Append(pFileName);
                selfTyped.IncludePaths.Add(includePath);
                selfTyped.IncludeWriteTimes.Add(File.GetLastWriteTime(includePath));

                //Load shader file into memory so D3D11 can compile it
                String source = File.ReadAllText(includePath, .. new String());
                *ppData = (u8*)source.Ptr;
                pBytes = (u32)source.Length;
                selfTyped._fileStrings.Add(source);

                return Win32.S_OK;
            }

            [CallingConvention(.Stdcall)]
            public static HRESULT Close(ID3DInclude* self, void* pData)
            {
                return 0;
            }
        }
	}
}