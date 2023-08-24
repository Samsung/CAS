
class ProjectFiles:

    PROJECT = '''<?xml version="1.0" encoding="UTF-8"?>
<projectDescription>
<name>{metaname}</name>
<comment></comment>
<projects>
</projects>
<buildSpec>
    <buildCommand>
        <name>org.eclipse.cdt.managedbuilder.core.genmakebuilder</name>
        <triggers>clean,full,incremental,</triggers>
        <arguments>
        </arguments>
    </buildCommand>
    <buildCommand>
        <name>org.eclipse.cdt.managedbuilder.core.ScannerConfigBuilder</name>
        <triggers>full,incremental,</triggers>
        <arguments>
        </arguments>
    </buildCommand>
</buildSpec>
<natures>
    <nature>org.eclipse.cdt.core.cnature</nature>
    <nature>org.eclipse.cdt.core.ccnature</nature>
    <nature>org.eclipse.cdt.managedbuilder.core.managedBuildNature</nature>
    <nature>org.eclipse.cdt.managedbuilder.core.ScannerConfigNature</nature>
</natures>
</projectDescription>'''
    
    USER_LANG_TYPE = '''\n            <language id="org.eclipse.cdt.core.{c_cpp}">{resources}\n            </language>'''
    
    USER_LANG_RESOURCE = '''\n                <resource project-relative-path="{path}">{entries}</resource>\n'''
    
    USER_LANG_SETTINGS_ENTRY = '\n                    <entry kind="macro" name={key} value={val} />'
    
    LANGUAGE = '''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<project>
  <configuration id="0.{timestamp}" name="Default">
      <extension point="org.eclipse.cdt.core.LanguageSettingsProvider">
          <provider-reference id="org.eclipse.cdt.core.ReferencedProjectsLanguageSettingsProvider" ref="shared-provider"/>
          <provider-reference id="org.eclipse.cdt.managedbuilder.core.MBSLanguageSettingsProvider" ref="shared-provider"/>
          <provider copy-of="extension" id="org.eclipse.cdt.managedbuilder.core.GCCBuildCommandParser"/>
          <provider build-parser-id="org.eclipse.cdt.managedbuilder.core.GCCBuildCommandParser" cdb-path="${{ProjDirPath}}/compile_commands.json" class="org.eclipse.cdt.managedbuilder.internal.language.settings.providers.CompilationDatabaseParser" id="org.eclipse.cdt.managedbuilder.core.CompilationDatabaseParser" name="Compilation Database Parser" prefer-non-shared="true" store-entries-with-project="true"/>
          <provider class="org.eclipse.cdt.core.language.settings.providers.LanguageSettingsGenericProvider" id="org.eclipse.cdt.ui.UserLanguageSettingsProvider" name="CDT User Setting Entries" prefer-non-shared="true" store-entries-with-project="true">{user_settings}
          </provider>          
      </extension>
  </configuration>
</project>'''
    CPROJECT = '''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<?fileVersion 4.0.0?><cproject storage_type_id="org.eclipse.cdt.core.XmlProjectDescriptionStorage">
    <storageModule moduleId="org.eclipse.cdt.core.settings">
        <cconfiguration id="0.{timestamp}">
            <storageModule buildSystemId="org.eclipse.cdt.managedbuilder.core.configurationDataProvider" id="0.{timestamp}" moduleId="org.eclipse.cdt.core.settings" name="Default">
                <externalSettings/>
                <extensions>
                    <extension id="org.eclipse.cdt.core.GASErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.GmakeErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.GLDErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.VCErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.CWDLocator" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.GCCErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                </extensions>
            </storageModule>
            <storageModule moduleId="cdtBuildSystem" version="4.0.0">
                <configuration buildProperties="" description="" id="0.{timestamp}" name="Default" parent="org.eclipse.cdt.build.core.prefbase.cfg">
                    <folderInfo id="0.{timestamp}." name="/" resourcePath="">
                        <toolChain id="org.eclipse.cdt.build.core.prefbase.toolchain.823993069" name="No ToolChain" resourceTypeBasedDiscovery="false" superClass="org.eclipse.cdt.build.core.prefbase.toolchain">
                            <targetPlatform id="org.eclipse.cdt.build.core.prefbase.toolchain.823993069.2016503727" name=""/>
                            <builder id="org.eclipse.cdt.build.core.settings.default.builder.1138103980" keepEnvironmentInBuildfile="false" managedBuildOn="false" name="Gnu Make Builder" superClass="org.eclipse.cdt.build.core.settings.default.builder"/>
                            <tool id="org.eclipse.cdt.build.core.settings.holder.libs.1645019473" name="holder for library settings" superClass="org.eclipse.cdt.build.core.settings.holder.libs"/>
                            <tool id="org.eclipse.cdt.build.core.settings.holder.1965094533" name="Assembly" superClass="org.eclipse.cdt.build.core.settings.holder">
                                <inputType id="org.eclipse.cdt.build.core.settings.holder.inType.1570609169" languageId="org.eclipse.cdt.core.assembly" languageName="Assembly" sourceContentType="org.eclipse.cdt.core.asmSource" superClass="org.eclipse.cdt.build.core.settings.holder.inType"/>
                            </tool>
                            <tool id="org.eclipse.cdt.build.core.settings.holder.2001520743" name="GNU C++" superClass="org.eclipse.cdt.build.core.settings.holder">
                                <inputType id="org.eclipse.cdt.build.core.settings.holder.inType.1135397011" languageId="org.eclipse.cdt.core.g++" languageName="GNU C++" sourceContentType="org.eclipse.cdt.core.cxxSource,org.eclipse.cdt.core.cxxHeader" superClass="org.eclipse.cdt.build.core.settings.holder.inType"/>
                            </tool>
                            <tool id="org.eclipse.cdt.build.core.settings.holder.497162202" name="GNU C" superClass="org.eclipse.cdt.build.core.settings.holder">
                                <inputType id="org.eclipse.cdt.build.core.settings.holder.inType.2032929481" languageId="org.eclipse.cdt.core.gcc" languageName="GNU C" sourceContentType="org.eclipse.cdt.core.cSource,org.eclipse.cdt.core.cHeader" superClass="org.eclipse.cdt.build.core.settings.holder.inType"/>
                            </tool>
                        </toolChain>
                    </folderInfo>
                </configuration>
            </storageModule>
            <storageModule moduleId="org.eclipse.cdt.core.externalSettings"/>
        </cconfiguration>
    </storageModule>
    <storageModule moduleId="cdtBuildSystem" version="4.0.0">
        <project id="{metaname}.null.452483973" name="{metaname}"/>
    </storageModule>
    <storageModule moduleId="scannerConfiguration">
        <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        <scannerConfigBuildInfo instanceId="0.{timestamp}">
            <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        </scannerConfigBuildInfo>
    </storageModule>
    <storageModule moduleId="org.eclipse.cdt.core.LanguageSettingsProviders"/>
</cproject>'''
