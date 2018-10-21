### This will do all the dynamic changing part

# options convert also happens here

# join / left participants

# dynamic message processing: layout change, bitrate change, idr request ..

# openresty as the starting entry, spwan new tasks here. 
  install: http://openresty.org/cn/linux-packages.html
    for centos, do the following
    sudo yum install yum-utils
    sudo yum-config-manager --add-repo https://openresty.org/package/centos/openresty.repo
    sudo yum install openresty openresty-resty openresty-opm

# task entry won't crash and reload, this is the baseline