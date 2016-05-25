### 发起异步http请求的一个php扩展

#### 依赖libevent库
#### install

    phpize
    ./configure --with-php-config=/usr/local/php/bin/php-config --with-libevent=/usr/local/libevent/ --enable-ahttp

#### demo

```php
$obj = new ahttp();
$obj->set_time_out(1000);
$obj->get('http://www.google.com');
$obj->post('http://example.com', array('data' => 'aaaaaa'));
$obj->get('http://example.com', array('header' => array('User-agent' => 'ahttp')));
$obj->wait_reply();
$arr = $obj->result();
```

返回值的顺序按发起的返回
