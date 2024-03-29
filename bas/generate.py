cdb_xml = """<?xml version="1.0" encoding="UTF-8"?>
<nodes>
%s</nodes>"""

link_template = """        <link>
            <name>%s</name>
            <type>%d</type>
            <location>%s</location>
        </link>"""

filter_template = """        <filter>
            <id>%d</id>
            <name>%s</name>
            <type>%d</type>
            <matcher>
                <id>org.eclipse.ui.ide.multiFilter</id>
                <arguments>1.0-name-matches-false-false-%s</arguments>
            </matcher>
        </filter>"""

project_template = """<?xml version="1.0" encoding="UTF-8"?>
<projectDescription>
    <name>%s</name>
    <comment>This file was autogenerated. Do not modify!</comment>
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
    <linkedResources>
%s
    </linkedResources>
    <filteredResources>%s
    </filteredResources>
</projectDescription>"""

cproject_template="""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<?fileVersion 4.0.0?><cproject storage_type_id="org.eclipse.cdt.core.XmlProjectDescriptionStorage">
    <storageModule moduleId="org.eclipse.cdt.core.settings">
        <cconfiguration id="0.834243849">
            <storageModule buildSystemId="org.eclipse.cdt.managedbuilder.core.configurationDataProvider" id="0.834243849" moduleId="org.eclipse.cdt.core.settings" name="Default">
                <externalSettings/>
            </storageModule>
            <storageModule moduleId="cdtBuildSystem" version="4.0.0">
                <configuration artifactName="${ProjName}" buildProperties="" description="" id="0.834243849" name="Default" parent="org.eclipse.cdt.apg.prefbase.cfg">
                    <folderInfo id="0.834243849." name="/" resourcePath="">
                        <toolChain id="org.eclipse.cdt.apg.prefbase.toolchain.306398286" name="No ToolChain" resourceTypeBasedDiscovery="false" superClass="org.eclipse.cdt.apg.prefbase.toolchain">
                            <targetPlatform id="org.eclipse.cdt.apg.prefbase.toolchain.306398286.9772260" name=""/>
                            <builder id="org.eclipse.cdt.apg.settings.default.builder.1223971741" keepEnvironmentInBuildfile="false" managedBuildOn="false" name="Gnu Make Builder" superClass="org.eclipse.cdt.apg.settings.default.builder"/>
                            <tool id="org.eclipse.cdt.apg.settings.holder.libs.2077815708" name="holder for library settings" superClass="org.eclipse.cdt.apg.settings.holder.libs"/>
                            <tool id="org.eclipse.cdt.apg.settings.holder.973439271" name="Assembly" superClass="org.eclipse.cdt.apg.settings.holder">
                                <inputType id="org.eclipse.cdt.apg.settings.holder.inType.772563436" languageId="org.eclipse.cdt.core.assembly" languageName="Assembly" sourceContentType="org.eclipse.cdt.core.asmSource" superClass="org.eclipse.cdt.apg.settings.holder.inType"/>
                            </tool>
                            <tool id="org.eclipse.cdt.apg.settings.holder.1435833824" name="GNU C++" superClass="org.eclipse.cdt.apg.settings.holder">
                                <inputType id="org.eclipse.cdt.apg.settings.holder.inType.1727973372" languageId="org.eclipse.cdt.core.g++" languageName="GNU C++" sourceContentType="org.eclipse.cdt.core.cxxSource,org.eclipse.cdt.core.cxxHeader" superClass="org.eclipse.cdt.apg.settings.holder.inType"/>
                            </tool>
                            <tool id="org.eclipse.cdt.apg.settings.holder.808320016" name="GNU C" superClass="org.eclipse.cdt.apg.settings.holder">
                                <inputType id="org.eclipse.cdt.apg.settings.holder.inType.1924293836" languageId="org.eclipse.cdt.core.gcc" languageName="GNU C" sourceContentType="org.eclipse.cdt.core.cSource,org.eclipse.cdt.core.cHeader" superClass="org.eclipse.cdt.apg.settings.holder.inType"/>
                            </tool>
                        </toolChain>
                    </folderInfo>
                    <sourceEntries>
                        <entry excluding="%s" flags="VALUE_WORKSPACE_PATH" kind="sourcePath" name=""/>
                    </sourceEntries>
                </configuration>
            </storageModule>
            <storageModule moduleId="org.eclipse.cdt.core.externalSettings"/>
        </cconfiguration>
    </storageModule>
    <storageModule moduleId="scannerConfiguration">
        <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        <scannerConfigBuildInfo instanceId="0.834243849">
            <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        </scannerConfigBuildInfo>
    </storageModule>
    <storageModule moduleId="org.eclipse.cdt.core.LanguageSettingsProviders"/>
    <storageModule moduleId="org.eclipse.cdt.internal.ui.text.commentOwnerProjectMappings"/>
</cproject>"""