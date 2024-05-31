#!/usr/bin/python3

#
# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import argparse
import sys
import os
import logging
import xml.etree.ElementTree as ET
import xml.etree.ElementInclude as EI
import xml.dom.minidom as MINIDOM

#
# Helper script that helps to feed at build time the XML Product Strategies Structure file file used
# by the engineconfigurable to start the parameter-framework.
# It prevents to fill them manually and avoid divergences with android.
#
# The Product Strategies Structure file is fed from the audio policy engine configuration file
# in order to discover all the strategies available for the current platform.
#           --audiopolicyengineconfigurationfile <path/to/audio_policy_engine_configuration.xml>
#
# At last, the output of the script shall be set also:
#           --outputfile <path/to/out/<system|vendor|odm>/etc/ProductStrategies.xml>
#

def parseArgs():
    argparser = argparse.ArgumentParser(description="Parameter-Framework XML \
                                        product strategies structure file generator.\n\
                                        Exit with the number of (recoverable or not) \
                                        error that occured.")
    argparser.add_argument('--audiopolicyengineconfigurationfile',
                           help="Android Audio Policy Engine Configuration file, Mandatory.",
                           metavar="(AUDIO_POLICY_ENGINE_CONFIGURATION_FILE)",
                           type=argparse.FileType('r'),
                           required=True)
    argparser.add_argument('--outputfile',
                           help="Product Strategies Structure output file, Mandatory.",
                           metavar="STRATEGIES_STRUCTURE_OUTPUT_FILE",
                           type=argparse.FileType('w'),
                           required=True)
    argparser.add_argument('--verbose',
                           action='store_true')

    return argparser.parse_args()


def generateXmlStructureFile(strategies, output_file):

    document = MINIDOM.Document()

    root = document.createElement('ComponentTypeSet')
    root.setAttributeNS("xmls", "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance")
    root.setAttribute("xsi:noNamespaceSchemaLocation=", "Schemas/ComponentTypeSet.xsd")
    document.appendChild(root)

    strategiesChild = document.createElement('ComponentType')
    strategiesChild.setAttribute('Name', 'ProductStrategies')
    strategiesChild.setAttribute('Description=', '')
    root.appendChild(strategiesChild)

    for strategy_name in strategies:
        context_mapping = "".join(map(str, ["Identifier:1000,Name:", strategy_name]))
        strategy_pfw_name = strategy_name.replace('STRATEGY_', '').lower()

        strategyChild = document.createElement('Component')
        strategyChild.setAttribute('Name', strategy_pfw_name)
        strategyChild.setAttribute('Type', 'ProductStrategy')
        strategyChild.setAttribute('Mapping', context_mapping)
        strategiesChild.appendChild(strategyChild)

    xml_str = document.toprettyxml(indent ="\t")
    output_file.write(xml_str)

def capitalizeLine(line):
    return ' '.join((w.capitalize() for w in line.split(' ')))


#
# Parse the audio policy configuration file and output a dictionary of device criteria addresses
#
def parseAndroidAudioPolicyEngineConfigurationFile(audiopolicyengineconfigurationfile):

    logging.info("Checking Audio Policy Engine Configuration file {}".format(
        audiopolicyengineconfigurationfile))
    #
    # extract all product strategies name from audio policy engine configuration file
    #
    strategy_names = []

    old_working_dir = os.getcwd()
    print("Current working directory %s" % old_working_dir)

    new_dir = os.path.join(old_working_dir, audiopolicyengineconfigurationfile.name)

    policy_engine_in_tree = ET.parse(audiopolicyengineconfigurationfile)
    os.chdir(os.path.dirname(os.path.normpath(new_dir)))

    print("new working directory %s" % os.getcwd())

    policy_engine_root = policy_engine_in_tree.getroot()
    EI.include(policy_engine_root)

    os.chdir(old_working_dir)

    for strategy in policy_engine_root.iter('ProductStrategy'):
        strategy_names.append(strategy.get('name'))

    return strategy_names


def main():
    logging.root.setLevel(logging.INFO)
    args = parseArgs()

    strategies = parseAndroidAudioPolicyEngineConfigurationFile(
        args.audiopolicyengineconfigurationfile)

    generateXmlStructureFile(strategies, args.outputfile)

# If this file is directly executed
if __name__ == "__main__":
    sys.exit(main())
