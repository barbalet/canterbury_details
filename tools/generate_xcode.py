#!/usr/bin/env python3
"""Generate the checked-in native app/CLI/smoke Xcode project without packages."""
import argparse,hashlib,json
from pathlib import Path
from xml.sax.saxutils import escape
import native_sources as n
ROOT=n.ROOT
PROJECT='CanterburyDetails.xcodeproj'
def identifier(s):return hashlib.sha1(s.encode()).hexdigest()[:24].upper()
def pbx(v,level=0):
    pad='\t'*level
    if isinstance(v,dict):return '{\n'+''.join(pad+'\t'+json.dumps(str(k))+' = '+pbx(x,level+1)+';\n' for k,x in v.items())+pad+'}'
    if isinstance(v,list):return '(\n'+''.join(pad+'\t'+pbx(x,level+1)+',\n' for x in v)+pad+')'
    return str(v) if type(v) is int else json.dumps(str(v))
def generate():
    objects={}
    def add(key,isa,**fields):
        ident=identifier(key);assert ident not in objects;objects[ident]={'isa':isa,**fields};return ident
    files={}
    def file(path,kind=None,source='SOURCE_ROOT'):
        if path in files:return files[path]
        ext=Path(path).suffix;types={'.c':'sourcecode.c.c','.h':'sourcecode.c.h','.m':'sourcecode.c.objc','.png':'image.png','.json':'text.json','.md':'net.daringfireball.markdown','.plist':'text.plist.xml','.py':'text.script.python'}
        files[path]=add('file:'+path,'PBXFileReference',lastKnownFileType=kind or types.get(ext,'text'),path=path,sourceTree=source);return files[path]
    native=n.sdk_sources();core=n.CORE;app=n.UI
    project_files=sorted(set(core+['src/main.c','tests/smoke.c']+app+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CityMap.h','ui/CitySession.h','ui/CityManagement.h','ui/Info.plist','tools/json2png2.c','tools/assembled_map.py','tools/build_basemap.py']))
    groups=[]
    for name,paths in [('Application',[p for p in project_files if p.startswith('ui/')]),('City model',[p for p in project_files if p.startswith('src/')]),('Smoke fixture',['tests/smoke.c']),('ApeSDK',native),('Map resources',n.RESOURCES),('Build and documentation',['README.md','PLAN.md','docs/XCODE.md','tools/native_sources.py','tools/native_profile.py','tools/build_basemap.py','tools/generate_xcode.py'])]:
        groups.append(add('group:'+name,'PBXGroup',name=name,children=[file(p) for p in paths],sourceTree='<group>'))
    cocoa=file('System/Library/Frameworks/Cocoa.framework','wrapper.framework','SDKROOT');groups.append(add('group:Frameworks','PBXGroup',name='Frameworks',children=[cocoa],sourceTree='<group>'))
    products=[];targets=[];schemes={}
    for kind,name,product in [('app','Canterbury Details','CanterburyDetails.app'),('cli','Canterbury Details CLI','CanterburyDetailsCLI'),('smoke','Canterbury Details Smoke','CanterburyDetailsSmoke')]:
        app_target=kind=='app';product_ref=add('product:'+kind,'PBXFileReference',explicitFileType='wrapper.application' if app_target else 'compiled.mach-o.executable',includeInIndex=0,path=product,sourceTree='BUILT_PRODUCTS_DIR');products.append(product_ref)
        builds=[]
        for path in n.sources(kind):builds.append(add('compile:'+kind+':'+path,'PBXBuildFile',fileRef=file(path)))
        sources=add('sources:'+kind,'PBXSourcesBuildPhase',buildActionMask=2147483647,files=builds,runOnlyForDeploymentPostprocessing=0)
        frameworks=add('frameworks:'+kind,'PBXFrameworksBuildPhase',buildActionMask=2147483647,files=[add('link:'+kind,'PBXBuildFile',fileRef=cocoa)] if app_target else [],runOnlyForDeploymentPostprocessing=0)
        script='set -eu\nexport PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"\ncd "$SRCROOT"\n"${CANTERBURY_PYTHON:-python3}" tools/build.py check\n"${CANTERBURY_PYTHON:-python3}" tools/compile_occupations.py --write\n"${CANTERBURY_PYTHON:-python3}" tools/compile_dsl_catalogue.py\n"${CANTERBURY_PYTHON:-python3}" tools/compile_controls.py\n"${CANTERBURY_PYTHON:-python3}" tools/checkpoint_abi.py\n'
        if app_target:script+='"${CANTERBURY_PYTHON:-python3}" tools/build_basemap.py --write\n"${CANTERBURY_PYTHON:-python3}" tools/compile_runtime.py --write\n"${CANTERBURY_PYTHON:-python3}" tools/compile_canterbury_dsl.py\n'
        phase=add('preflight:'+kind,'PBXShellScriptBuildPhase',name='Audit local profile and render json2png map' if app_target else 'Audit local profile',alwaysOutOfDate=1,buildActionMask=2147483647,files=[],inputPaths=[],outputPaths=['$(SRCROOT)/build/map/CanterburyMap.png','$(SRCROOT)/build/map/CanterburyMapBackground.png','$(SRCROOT)/build/map/render.json','$(SRCROOT)/build/map/buildings-enriched.json','$(SRCROOT)/build/content/CanterburyCity.bin','$(SRCROOT)/build/content/CanterburyScenario.json'] if app_target else [],runOnlyForDeploymentPostprocessing=0,shellPath='/bin/sh',shellScript=script,showEnvVarsInLog=0)
        phases=[phase,sources,frameworks]
        if app_target:phases.append(add('resources:'+kind,'PBXResourcesBuildPhase',buildActionMask=2147483647,files=[add('resource:'+p,'PBXBuildFile',fileRef=file(p)) for p in n.RESOURCES],runOnlyForDeploymentPostprocessing=0))
        configs=[]
        for configuration in ('Debug','Release'):
            settings={'PRODUCT_NAME':product.removesuffix('.app'),'CODE_SIGNING_ALLOWED':'NO','CODE_SIGN_IDENTITY':'-','ENABLE_HARDENED_RUNTIME':'NO','GENERATE_INFOPLIST_FILE':'NO','GCC_OPTIMIZATION_LEVEL':'0' if configuration=='Debug' else '2','ONLY_ACTIVE_ARCH':'YES' if configuration=='Debug' else 'NO'}
            if app_target:settings.update(INFOPLIST_FILE='ui/Info.plist',PRODUCT_BUNDLE_IDENTIFIER='org.canterbury.details',COMBINE_HIDPI_IMAGES='YES',LD_RUNPATH_SEARCH_PATHS=['$(inherited)','@executable_path/../Frameworks'])
            configs.append(add('config:'+kind+configuration,'XCBuildConfiguration',name=configuration,buildSettings=settings))
        config_list=add('configs:'+kind,'XCConfigurationList',buildConfigurations=configs,defaultConfigurationIsVisible=0,defaultConfigurationName='Debug')
        target=add('target:'+kind,'PBXNativeTarget',name=name,productName=product.removesuffix('.app'),productReference=product_ref,productType='com.apple.product-type.application' if app_target else 'com.apple.product-type.tool',buildConfigurationList=config_list,buildPhases=phases,buildRules=[],dependencies=[]);targets.append(target);schemes[name]=(target,product)
    products_group=add('group:Products','PBXGroup',name='Products',children=products,sourceTree='<group>');groups.append(products_group)
    main_group=add('group:main','PBXGroup',children=groups,sourceTree='<group>')
    configurations=[]
    for configuration in ('Debug','Release'):
        settings={'SDKROOT':'macosx','SUPPORTED_PLATFORMS':'macosx','MACOSX_DEPLOYMENT_TARGET':'14.0','GCC_C_LANGUAGE_STANDARD':'c11','CLANG_ENABLE_OBJC_ARC':'YES','CLANG_ENABLE_MODULES':'YES','GCC_PREPROCESSOR_DEFINITIONS':['$(inherited)','CANTERBURY_SIM=1','BRONZE_SIM=1','HIDE_THUNDER=1','COMMAND_LINE_EXPLICIT=1']+(['DEBUG=1'] if configuration=='Debug' else []),'GCC_PREFIX_HEADER':'$(SRCROOT)/src/cd_api.h','GCC_PRECOMPILE_PREFIX_HEADER':'NO','HEADER_SEARCH_PATHS':['$(SRCROOT)/apesdk','$(SRCROOT)/src'],'OTHER_LDFLAGS':['$(inherited)','-lz','-lm','-lpthread'],'ENABLE_USER_SCRIPT_SANDBOXING':'NO','CANTERBURY_PYTHON':'python3','DEBUG_INFORMATION_FORMAT':'dwarf' if configuration=='Debug' else 'dwarf-with-dsym','GCC_GENERATE_DEBUGGING_SYMBOLS':'YES','GCC_TREAT_WARNINGS_AS_ERRORS':'NO'}
        configurations.append(add('projectconfig:'+configuration,'XCBuildConfiguration',name=configuration,buildSettings=settings))
    config_list=add('projectconfigs','XCConfigurationList',buildConfigurations=configurations,defaultConfigurationIsVisible=0,defaultConfigurationName='Debug')
    project=add('project','PBXProject',attributes={'LastUpgradeCheck':'1600','BuildIndependentTargetsInParallel':'YES'},buildConfigurationList=config_list,compatibilityVersion='Xcode 14.0',developmentRegion='en',hasScannedForEncodings=0,knownRegions=['en','Base'],mainGroup=main_group,productRefGroup=products_group,projectDirPath='',projectRoot='',targets=targets)
    outputs={PROJECT+'/project.pbxproj':'// !$*UTF8*$!\n'+pbx({'archiveVersion':1,'classes':{},'objectVersion':56,'objects':objects,'rootObject':project})+'\n'}
    for name,(target,product) in schemes.items():
        ref=f'<BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="{escape(product)}" BlueprintName="{escape(name)}" ReferencedContainer="container:{PROJECT}"/>'
        outputs[PROJECT+'/xcshareddata/xcschemes/'+name+'.xcscheme']=f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3">
 <BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">{ref}</BuildActionEntry></BuildActionEntries></BuildAction>
 <TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables/></TestAction>
 <LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="YES" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="NO"><BuildableProductRunnable runnableDebuggingMode="0">{ref}</BuildableProductRunnable><CommandLineArguments>{''.join('<CommandLineArgument argument="'+arg+'" isEnabled="YES"/>' for arg in ('--people','32','--minutes','5')) if name.endswith('CLI') else ''}</CommandLineArguments></LaunchAction>
 <ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0">{ref}</BuildableProductRunnable></ProfileAction>
 <AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
'''
    return outputs

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    for path,text in generate().items():
        file=ROOT/path
        if a.write:file.parent.mkdir(parents=True,exist_ok=True);file.write_text(text)
        elif not file.is_file() or file.read_text()!=text:raise SystemExit('Stale Xcode project: run python3 tools/generate_xcode.py --write')
    print('PASS native Xcode app/CLI/smoke project and shared schemes')
if __name__=='__main__':main()
