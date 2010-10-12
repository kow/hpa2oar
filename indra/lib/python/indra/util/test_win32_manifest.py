#!/usr/bin/env python
# @file test_win32_manifest.py
# @brief Test an assembly binding version and uniqueness in a windows dll or exe.  
#
# $LicenseInfo:firstyear=2009&license=viewerlgpl$
# Second Life Viewer Source Code
# Copyright (C) 2010, Linden Research, Inc.
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License only.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
# 
# Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
# $/LicenseInfo$

import sys, os
import tempfile
from xml.dom.minidom import parse

class AssemblyTestException(Exception):
    pass

class NoManifestException(AssemblyTestException):
    pass

class MultipleBindingsException(AssemblyTestException):
    pass

class UnexpectedVersionException(AssemblyTestException):
    pass

class NoMatchingAssemblyException(AssemblyTestException):
    pass

def get_HKLM_registry_value(key_str, value_str):
    import _winreg
    reg = _winreg.ConnectRegistry(None, _winreg.HKEY_LOCAL_MACHINE)
    key = _winreg.OpenKey(reg, key_str)
    value = _winreg.QueryValueEx(key, value_str)[0]
    #print 'Found: %s' % value
    return value
        
def find_vc_dir(vcversion):
    supported_products = ('VisualStudio', 'VCExpress')
    value_str = (r'ProductDir')
    
    for product in supported_products:
        key_str = (r'SOFTWARE\Microsoft\%s\%s\Setup\VC' %
                    (product, vcversion))
        try:
            return get_HKLM_registry_value(key_str, value_str)
        except WindowsError, err:
            x64_key_str = (r'SOFTWARE\Wow6432Node\Microsoft\%s\%s\Setup\VS' %
                       (product, vcversion))
            try:
                return get_HKLM_registry_value(x64_key_str, value_str)
            except:
                print >> sys.stderr, "Didn't find MS %s version %s " % (product,vcversion)
                sys.stdout.flush()
    raise
   
# This function attempts to find a working mt.exe if it has not been found in the Visual Studio dir
# by find_vc_dir(), 2005 Express has a working mt.exe in its install folder, 2008 Express does not
def find_mt_via_sdk_dir():
    supported_sdks = ('v6.0A', 'v7.0', 'v7.0a', 'v7.1')
    value_str = (r'InstallationFolder')
    
    for sdk in supported_sdks:
        key_str = (r'SOFTWARE\Microsoft\Microsoft SDKs\Windows\%s' % sdk)
        try:
            path = get_HKLM_registry_value(key_str, value_str)
            mt_path = '\"%sbin\\mt.exe\"' % path
            os.path.isfile(mt_path)
            return mt_path    
        except:
            print >> sys.stderr, "Failed to find mt.exe via %s SDK" % sdk
            
    print >> sys.stderr, "ERROR: Failed to find mt.exe via any SDK backup route"
    
def find_mt_path(vcversion):
    vc_dir = find_vc_dir(vcversion)
    mt_path = '\"%sbin\\mt.exe\"' % vc_dir
    try:
        os.path.isfile(fname)
    except:
        print >> sys.stderr, "WARNING: mt.exe was not found via Visual Studio, using SDK path fallback"
        try:
            mt_path = find_mt_via_sdk_dir()
        except:
            print >> sys.stderr, "ERROR: mt.exe was not found"
            raise
    return mt_path
    
def test_assembly_binding(src_filename, assembly_name, assembly_ver, vcversion):
    print "checking %s dependency %s..." % (src_filename, assembly_name)

    (tmp_file_fd, tmp_file_name) = tempfile.mkstemp(suffix='.xml')
    tmp_file = os.fdopen(tmp_file_fd)
    tmp_file.close()

    mt_path = find_mt_path(vcversion)
    resource_id = ""
    if os.path.splitext(src_filename)[1].lower() == ".dll":
       resource_id = ";#2"
    system_call = '%s -nologo -inputresource:%s%s -out:%s > NUL' % (mt_path, src_filename, resource_id, tmp_file_name)
    print "Executing: %s" % system_call
    mt_result = os.system(system_call)
    if mt_result == 31:
        print "No manifest found in %s" % src_filename
        raise NoManifestException()

    manifest_dom = parse(tmp_file_name)
    nodes = manifest_dom.getElementsByTagName('assemblyIdentity')

    versions = list()
    for node in nodes:
        if node.getAttribute('name') == assembly_name:
            versions.append(node.getAttribute('version'))

    if len(versions) == 0:
        print "No matching assemblies found in %s" % src_filename
        raise NoMatchingAssemblyException()
        
    elif len(versions) > 1:
        print "Multiple bindings to %s found:" % assembly_name
        print versions
        print 
        raise MultipleBindingsException(versions)

    elif versions[0] != assembly_ver:
        print "Unexpected version found for %s:" % assembly_name
        print "Wanted %s, found %s" % (assembly_ver, versions[0])
        print
        raise UnexpectedVersionException(assembly_ver, versions[0])
            
    os.remove(tmp_file_name)
    
    print "SUCCESS: %s OK!" % src_filename
    print
  
if __name__ == '__main__':

    print
    print "Running test_win32_manifest.py..."
    
    usage = 'test_win32_manfest <srcFileName> <assemblyName> <assemblyVersion>'

    try:
        src_filename = sys.argv[1]
        assembly_name = sys.argv[2]
        assembly_ver = sys.argv[3]
    except:
        print "Usage:"
        print usage
        print
        raise
    
    test_assembly_binding(src_filename, assembly_name, assembly_ver)

    
