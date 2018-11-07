import subprocess
import os
import json
import ntpath
import datetime

def load_json_obj_from_config(file):
    if (len(file) <= 0):
        print 'please input json format file.'
        return None
    
    with open(file) as f:
        cfg_json = json.loads(f.read())
    
    return cfg_json
    
class Fieldbus(object):
    
    def __init__(self):
        self.exe_name = 'trina-P1master'
        self.exe_path = '/usr/sbin/'
        self.config_path = '/etc/trina-P1master/'
        self.template_path = self.config_path + 'template.d/'
        self.config = self.config_path + 'configuration.json'
    
    def check(self):
        if not (os.path.isfile(self.config)):
            print 'protocol configuration file[%s] is not exist' %(self.config)
            return -1;
            
    def check_tag_valid(self, tag_jobj):
        if not ('id' in tag_jobj): 
            print 'id is not exist in tag json context.'
            return -1
        if not ('op' in tag_jobj): 
            print 'op is not exist in tag json context.'
            return -1
        if not ('type' in tag_jobj): 
            print 'type is not exist in tag json context.'
            return -1
        if not ('requestTimeoutMs' in tag_jobj): 
            print 'requestTimeoutMs is not exist in tag json context.'
            return -1
        if not ('pollingPeriodMs' in tag_jobj): 
            print 'pollingPeriodMs is not exist in tag json context.'
            return -1
        if not ('address' in tag_jobj): 
            print 'address is not exist in tag json context.'
            return -1
        if not ('cid1' in tag_jobj): 
            print 'cid1 is not exist in tag json context.'
            return -1
        if not ('cid2' in tag_jobj): 
            print 'cid2 is not exist in tag json context.'
            return -1
        if not ('length' in tag_jobj): 
            print 'length is not exist in tag json context.'
            return -1
        if not ('info' in tag_jobj): 
            print 'info is not exist in tag json context.'
            return -1
        return 0
        
    def start(self):
        os.system('service ' + self.exe_name + ' start')
    
    def stop(self):
        os.system('service ' + self.exe_name + ' stop')
        
    def add_device(self, json_obj):
        with open(self.config) as f:
            data = json.loads(f.read())
        
        for device in data['deviceList']:
            if (device['id'] == json_obj['id']):
                print 'Device id[%s] is exist' %(json_obj['id'])
                return -1
            elif (device['name'] == json_obj['name']):
                print 'Device name[%s] is exist' %(json_obj['name'])
                return -1
        
        if not (os.path.isfile(self.template_path + json_obj['templateName'])):
            print 'Template[%s] is not exist' %(json_obj['templateName'])
            return -1
            
        data['deviceList'].append(json_obj)
        
        # todo : user hook 
        
        with open(self.config, 'w') as outfile:
            json.dump(data, outfile, indent=4)
        
        print json.dumps(json_obj)

        return 0
        
    def edit_device(self, json_objs):
        with open(self.config) as f:
            data = json.loads(f.read())
        
        success_list = {'deviceList':[]}

        for edit_device in json_objs['deviceList']:
            index = 0
            for exist_device in data['deviceList']:
                if (edit_device['id'] == exist_device['id']):
                    del data['deviceList'][index]
                    data['deviceList'].append(edit_device)
                    success_list['deviceList'].append(edit_device)
                index += 1
        
        # todo : user hook 
        
        with open(self.config, 'w') as outfile:
            json.dump(data, outfile, indent=4)
        
        # print success edit device list 
        print json.dumps(success_list)
        return 0
    
    def remove_device(self, json_objs):
        with open(self.config) as f:
            data = json.loads(f.read())

        success_list = {'deviceList':[]}
        for remove_device_item in json_objs['deviceList']:
            index = 0
            for exist_device_item in data['deviceList']:
                if (remove_device_item['id'] == exist_device_item['id']):
                    del data['deviceList'][index]
                    success_list['deviceList'].append(remove_device_item)
                    break
                else:
                    index += 1
            
        # todo : user hook

        with open(self.config, 'w') as outfile:
            json.dump(data, outfile, indent=4)

        print json.dumps(success_list)

        return 0

    def get_template_list(self):
        template_all = {
            "list": []
        }
        for f in os.listdir(self.template_path):
            file_path = os.path.join(self.template_path, f)
            if not os.path.isfile(file_path):
                continue
            with open(file_path) as f:
                jobj = json.loads(f.read())
                if not 'templateName' in jobj:
                    continue
                template_all['list'].append({
                    "templateName": jobj['templateName'],
                    "tagList": jobj['tagList']
                })
        return template_all

    def get_template(self, template_name):
        for f in os.listdir(self.template_path): 
            if not template_name == f:
                continue
                
            file_path = os.path.join(self.template_path, f)
            if not os.path.isfile(file_path):
                continue
                
            with open(file_path) as f:
                return json.loads(f.read())
        
    def add_template(self, file_path):        
        tmp_json = load_json_obj_from_config(file_path)
        if (tmp_json == None): 
            print 'add template file fail.[invalid json].'
            return -1
        if not ('templateName' in tmp_json):
            print 'template file fail.[templateName not found].'
            return -1
        
        dest = self.template_path + tmp_json['templateName']
        if (os.path.isfile(dest)):
            print 'template file[%s] has already exist.' %(dest)
            return -1
            
        template_name = tmp_json['templateName']
        for f in os.listdir(self.template_path):
            subf_json = load_json_obj_from_config(self.template_path+f)
            if (subf_json == None): continue
            if not ('templateName' in subf_json):continue
            if (subf_json['templateName'] == template_name):
                print 'Template id[%s] is already exists' %(template_name)
                return -1;
        
        with open(file_path) as f:
            data = json.loads(f.read())
        
        for tag in data['tagList']:
            if (self.check_tag_valid(tag) != 0):
                print 'Template[%s] tagList is not valid.' %(file_path)
                return -1
        
        # todo : user hook 
        os.system('rm -rf ' + dest)

        with open(dest, 'w') as outfile:
            json.dump(data, outfile, indent=4)

        return 0
        
    def edit_template(self, file_path):
        tmp_json = load_json_obj_from_config(file_path)
        if (tmp_json == None): 
            print 'edit template file fail.[invalid json].'
            return -1
        
        if not ('templateName' in tmp_json):
            print 'template file fail.[templateName not found].'
            return -1
        
        dest = self.template_path + tmp_json['templateName']
        if not (os.path.isfile(dest)):
            print 'template file[%s] is exist.' %(dest)
            return -1
        
        current_tmp_file = ''
        template_name = tmp_json['templateName']
        for f in os.listdir(self.template_path):
            subf_json = load_json_obj_from_config(self.template_path+f)
            if (subf_json == None): continue
            if not ('templateName' in subf_json):continue
            if (subf_json['templateName'] != template_name):continue
            current_tmp_file = f
            break
            
        # if (current_tmp_file != ntpath.basename(file_path)):
        #     print 'update file[%s] name ie not equal with org file[%s].' %(file_path, current_tmp_file)
        #     return -1
            
        with open(file_path) as f:
            data = json.loads(f.read())
        
        for tag in data['tagList']:
            if (self.check_tag_valid(tag) != 0):
                print 'Template[%s] tagList is not valid.' %(file_path)
                return -1
        
        # todo : user hook 
        os.system('rm -rf ' + dest)

        with open(dest, 'w') as outfile:
            json.dump(data, outfile, indent=4)
        
        return 0
        
    def remove_template(self, template_name):
        current_tmp_file = ''
        for f in os.listdir(self.template_path):
            subf_json = load_json_obj_from_config(self.template_path+f)
            if (subf_json == None): continue
            if not ('templateName' in subf_json):continue
            if (subf_json['templateName'] != template_name):continue
            current_tmp_file = self.template_path+f
            break
        
        if (len(current_tmp_file) <= 0):
            print 'template id[%s] is not found in folder[%s].' %(template_name, self.template_path)
            return -1
        
        os.system('rm -rf ' + current_tmp_file)
        return 0
        
    def get_status(self, protocol_name):
        p = subprocess.Popen('systemctl show ' + self.exe_name +' -pActiveState', shell=True,
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE)

        service_status_str, _ = p.communicate()
        if (len(service_status_str.split('=')) >= 2):
            sstr = (service_status_str.split('=')[1]).strip()
            if (sstr.find('deactivating') != -1 or sstr.find('activating') != -1 or sstr.find('reloading') != -1):
                return "{ \"serviceStatus\" : \"restarting\", \"at\" : \""+ str(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')) +"\" }"
            elif (sstr == 'inactive'):
                return "{ \"serviceStatus\" : \"inactive\", \"at\" : \""+ str(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')) +"\" }"
            elif (sstr == 'failed'):
                return "{ \"serviceStatus\" : \"failed\", \"at\" : \""+ str(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')) +"\" }"

        p = subprocess.Popen('mxfbcli  -p ' + protocol_name + ' -a', shell=True,
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE)

        readlines, _ = p.communicate()

        if (p.returncode != 0):
            return "{ \"serviceStatus\" : \"failed\", \"at\" : \""+ str(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')) +"\" }"

        try:
            status_json = json.loads(readlines.strip())
            status_json['serviceStatus'] = 'active'
            status_str = json.dumps(status_json)
        except:
            status_str = "{ \"serviceStatus\" : \"failed\", \"at\" : \""+ str(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')) +"\" }"

        return status_str

    def device_test(self, protocol_name, device_jfile):
        
        f = os.popen('mxfbcli -p ' + protocol_name + ' -f ' + device_jfile)
        output = f.read()
        
        if len(output) <= 0:
            print 'test device fail'
            return ""
        
        return output

    def get_device_list(self, protocol_name):
        device_tag_dict = {}
        tag_json_str = self.get_status(protocol_name)
        status_json = json.loads(tag_json_str)
        if (status_json['serviceStatus'] == 'active'):
            for device_item in status_json['deviceList']:
                device_tag_dict[device_item['name']] = [device_item['totalTags'], device_item['successTags']]
        
        with open(self.config) as f: # get device list from json configuration
            cfg_json = json.loads(f.read())
        
        for device_item in cfg_json['deviceList']:
            device_item['totalTags'] = 0
            device_item['successTags'] = 0
            
            for device_name, tag_status in device_tag_dict.items():
                if (device_item['name'] == device_name):
                    device_item['totalTags'] = tag_status[0]
                    device_item['successTags'] = tag_status[1]

        device_list = {'deviceList': []}
        device_list['deviceList'] = cfg_json['deviceList']

        return device_list

    def get_tag_list(self):
        with open(self.config) as f:
            cfg_json = json.loads(f.read())

        tags = json.loads('{\"tagList\" : []}')
        for item in cfg_json['deviceList']:
            template_name = item['templateName']
            with open(self.template_path + template_name) as template:
                tmp_json = json.loads(template.read())
                if (tmp_json == None):
                    continue
                if not ('templateName' in tmp_json):
                    continue
                if (tmp_json['templateName'] != template_name):
                    continue
                for tag in tmp_json['tagList']:
                    new_tag = {}
                    new_tag['srcName'] = item['name']
                    new_tag['tagName'] = tag['id']
                    new_tag['duration'] = tag['pollingPeriodMs']
                    new_tag['dataType'] = tag['type']
                    if not 'uint' in tag:
                        new_tag['dataUnit'] = ''
                    else:
                        new_tag['dataUnit'] = tag['unit']
                    
                    if tag['op'] == 'read':
                        new_tag['access'] = 'ro'
                    elif tag['op'] == 'write':
                        new_tag['access'] = 'wo'
                    else:
                        new_tag['access'] = 'rw'
                    
                    tags['tagList'].append(new_tag)
        return tags
